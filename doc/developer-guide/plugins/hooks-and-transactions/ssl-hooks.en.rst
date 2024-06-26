.. Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
   distributed with this work for additional information
   regarding copyright ownership.  The ASF licenses this file
   to you under the Apache License, Version 2.0 (the
   "License"); you may not use this file except in compliance
   with the License.  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing,
   software distributed under the License is distributed on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.  See the License for the
   specific language governing permissions and limitations
   under the License.

.. include:: ../../../common.defs
.. default-domain:: cpp

.. _developer-plugins-ssl-hooks:

TLS User Agent Hooks
********************

In addition to the HTTP oriented hooks, a plugin can add hooks (by calling :func:`TSHttpHookAdd`)
to trigger code during the TLS handshake with the user agent.  This TLS handshake occurs well
before the HTTP transaction is available, so a separate state machine is required to track the
TLS hooks.

TLS Hooks
---------

In all cases, the hook callback has the following signature.

.. function:: int SSL_callback(TSCont contp, TSEvent event, void * edata)

The edata parameter is a TSVConn object.

The following actions are valid from these callbacks.

  * Fetch the SSL object associated with the connection - :func:`TSVConnSslConnectionGet`
  * Set a connection to blind tunnel - :func:`TSVConnTunnel`
  * Re-enable the ssl connection - :func:`TSVConnReenable`
  * Find SSL context by name - :func:`TSSslContextFindByName`
  * Find SSL context by address - :func:`TSSslContextFindByAddr`
  * Determine whether the TSVConn is really representing a SSL connection - :func:`TSVConnIsSsl`

TS_VCONN_START_HOOK
------------------------

This hook is invoked after the client has connected to ATS and before the SSL handshake is started,
i.e., before any bytes have been read from the client. The data for the callback is a TSVConn
instance which represents the client connection. There is no HTTP transaction as no headers have
been read.

In theory this hook could apply and be useful for non-SSL connections as well, but at this point
this hook is only called in the SSL sequence.

The TLS handshake processing will not proceed until :func:`TSVConnReenable()` is called either
from within the hook callback or from another piece of code.

TS_VCONN_CLOSE_HOOK
------------------------

This hook is invoked after the SSL handshake is done and when the IO is closing. The TSVConnArgs
should be cleaned up here. A callback at this point must re-enable.

TS_SSL_CLIENT_HELLO_HOOK
------------------------

This hook is called when the client hello arrived for the TLS handshake. If called it will always be
called after TS_VCONN_START_HOOK. The plugin callback can execute code to examine client hello
information.

TLS handshake processing will pause until the hook callback executes :func:`TSVConnReenable()`.

TS_SSL_SERVERNAME_HOOK
----------------------

This hook is called if the client provides SNI information in the SSL handshake. If called it will
always be called after TS_VCONN_START_HOOK.

The Traffic Server core first evaluates the settings in the ssl_multicert.config file based on the
server name. Then the core SNI callback executes the plugin registered SNI callback code. The plugin
callback can access the servername by calling the OpenSSL function SSL_get_servername().

Processing will continue regardless of whether the hook callback executes
:func:`TSVConnReenable()` since the OpenSSL implementation does not allow for pausing processing
during the OpenSSL servername callback.

TS_SSL_CERT_HOOK
----------------

This hook is called as the server certificate is selected for the TLS handshake. The plugin callback
can execute code to create or select the certificate that should be used for the TLS handshake.
This will override the default Traffic Server certificate selection.

If you are running with OpenSSL 1.0.2 or later, you can control whether the TLS handshake processing
will continue after the certificate hook callback execute by calling :func:`TSVConnReenable()` or
not.  The TLS handshake processing will not proceed until :func:`TSVConnReenable()` is called.

It may be useful to delay the TLS handshake processing if other resources must be consulted to
select or create a certificate.

TS_SSL_VERIFY_CLIENT_HOOK
-------------------------

This hook is called when a client connects to |TS| and presents a client certificate in the case of
a mutual TLS handshake.  The callback can use the TSVConn argument and fetch the TSSslVerifyCTX
object using the :func:`TSVConnSslVerifyCTXGet()` method and fetch the peer's certificates to make
any additional checks.

Processing will continue regardless of whether the hook callback executes
:func:`TSVConnReenable()` since the OpenSSL implementation does not allow for pausing processing
during the certificate verify callback.  The plugin can use the :func:`TSVConnReenableEx()`
function to pass in the :enumerator:`TS_EVENT_ERROR` and stop the TLS handshake.

TS_SSL_VERIFY_SERVER_HOOK
-------------------------

This hook is called when a Traffic Server connects to an origin and the origin presents a
certificate.  The callback can use the TSVConn argument and fetch the TSSslVerifyCTX object using
the :func:`TSVConnSslVerifyCTXGet()` method and fetch the peer's certificates to make any
additional checks.

Processing will continue regardless of whether the hook callback executes
:func:`TSVConnReenable()` since the OpenSSL implementation does not allow for pausing processing
during the certificate verify callback.  The plugin can use the :func:`TSVConnReenableEx()`
function to pass in the :enumerator:`TS_EVENT_ERROR` and

TS_VCONN_OUTBOUND_START_HOOK
----------------------------

This hook is invoked after ATS has connected to the upstream server and before the SSL handshake has
started.  This gives the plugin the option of overriding the default SSL connection options on the
SSL object.

In theory this hook could apply and be useful for non-SSL connections as well, but at this point
this hook is only called in the SSL sequence.

The TLS handshake processing will not proceed until :func:`TSVConnReenable()` is called either
from within the hook callback or from another piece of code.

TS_VCONN_OUTBOUND_CLOSE_HOOK
-----------------------------

This hook is invoked after the SSL handshake is done and right before the outbound connection
closes.  A callback at this point must re-enable.

TLS Inbound Hook State Diagram
------------------------------

.. graphviz::
   :alt: TLS Inbound Hook State Diagram

   digraph tls_hook_state_diagram{
     HANDSHAKE_HOOKS_PRE -> TS_VCONN_START_HOOK;
     HANDSHAKE_HOOKS_PRE -> TS_SSL_CERT_HOOK;
     HANDSHAKE_HOOKS_PRE -> TS_SSL_SERVERNAME_HOOK;
     HANDSHAKE_HOOKS_PRE -> HANDSHAKE_HOOKS_DONE;
     TS_VCONN_START_HOOK -> HANDSHAKE_HOOKS_PRE_INVOKE;
     HANDSHAKE_HOOKS_PRE_INVOKE -> TSVConnReenable;
     TSVConnReenable -> HANDSHAKE_HOOKS_PRE;
     TS_SSL_CLIENT_HELLO_HOOK -> HANDSHAKE_HOOKS_CLIENT_HELLO_INVOKE;
     HANDSHAKE_HOOKS_CLIENT_HELLO_INVOKE -> TSVConnReenable2;
     TSVConnReenable2 -> HANDSHAKE_HOOKS_CLIENT_HELLO;
     HANDSHAKE_HOOKS_CLIENT_HELLO -> TS_SSL_CLIENT_HELLO_HOOK;
     HANDSHAKE_HOOKS_CLIENT_HELLO -> TS_SSL_SERVERNAME_HOOK;
     TS_SSL_SERVERNAME_HOOK -> HANDSHAKE_HOOKS_SNI;
     HANDSHAKE_HOOKS_SNI -> TS_SSL_SERVERNAME_HOOK;
     HANDSHAKE_HOOKS_SNI -> TS_SSL_CERT_HOOK;
     HANDSHAKE_HOOKS_SNI -> HANDSHAKE_HOOKS_DONE;
     HANDSHAKE_HOOKS_CERT -> TS_SSL_CERT_HOOK;
     TS_SSL_CERT_HOOK -> HANDSHAKE_HOOKS_CERT_INVOKE;
     HANDSHAKE_HOOKS_CERT_INVOKE -> TSVConnReenable3;
     TSVConnReenable3 -> HANDSHAKE_HOOKS_CERT;

     HANDSHAKE_HOOKS_CERT -> TS_SSL_VERIFY_CLIENT_HOOK;
     HANDSHAKE_HOOKS_SNI -> TS_SSL_VERIFY_CLIENT_HOOK;
     HANDSHAKE_HOOKS_PRE -> TS_SSL_VERIFY_CLIENT_HOOK;
     TS_SSL_VERIFY_CLIENT_HOOK -> HANDSHAKE_HOOKS_VERIFY;
     HANDSHAKE_HOOKS_VERIFY -> TS_SSL_VERIFY_CLIENT_HOOK;
     HANDSHAKE_HOOKS_VERIFY -> HANDSHAKE_HOOKS_DONE;

     HANDSHAKE_HOOKS_CERT -> HANDSHAKE_HOOKS_DONE;
     HANDSHAKE_HOOKS_DONE -> TS_VCONN_CLOSE_HOOK;
     TS_VCONN_CLOSE_HOOK -> HANDSHAKE_HOOKS_DONE;

     HANDSHAKE_HOOKS_PRE [shape=box];
     HANDSHAKE_HOOKS_PRE_INVOKE [shape=box];
     HANDSHAKE_HOOKS_CLIENT_HELLO [shape=box];
     HANDSHAKE_HOOKS_CLIENT_HELLO_INVOKE [shape=box];
     HANDSHAKE_HOOKS_SNI [shape=box];
     HANDSHAKE_HOOKS_VERIFY [shape=box];
     HANDSHAKE_HOOKS_CERT [shape=box];
     HANDSHAKE_HOOKS_CERT_INVOKE [shape=box];
     HANDSHAKE_HOOKS_DONE [shape=box];
   }

TLS Outbound Hook State Diagram
-------------------------------

.. graphviz::
   :alt: TLS Outbound Hook State Diagram

   digraph tls_hook_state_diagram{
     HANDSHAKE_HOOKS_OUTBOUND_PRE -> TS_VCONN_OUTBOUND_START_HOOK;
     TS_VCONN_OUTBOUND_START_HOOK -> HANDSHAKE_HOOKS_OUTBOUND_PRE_INVOKE;
     HANDSHAKE_HOOKS_OUTBOUND_PRE_INVOKE-> TSVConnReenable;
     TSVConnReenable -> HANDSHAKE_HOOKS_OUTBOUND_PRE;
     HANDSHAKE_HOOKS_OUTBOUND_PRE -> TS_SSL_VERIFY_SERVER_HOOK;
     TS_SSL_VERIFY_SERVER_HOOK -> HANDSHAKE_HOOKS_OUTBOUND_PRE;
     HANDSHAKE_HOOKS_OUTBOUND_PRE -> TS_VCONN_OUTBOUND_CLOSE;
     TS_VCONN_OUTBOUND_CLOSE -> HANDSHAKE_HOOKS_OUTBOUND_PRE;
     TS_VCONN_OUTBOUND_CLOSE -> HANDSHAKE_HOOKS_DONE;

     HANDSHAKE_HOOKS_OUTBOUND_PRE [shape=box];
     HANDSHAKE_HOOKS_OUTBOUND_PRE_INVOKE [shape=box];
     HANDSHAKE_HOOKS_DONE [shape=box];
   }

