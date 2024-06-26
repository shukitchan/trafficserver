.. Licensed to the Apache Software Foundation (ASF) under one or more
   contributor license agreements.  See the NOTICE file distributed
   with this work for additional information regarding copyright
   ownership.  The ASF licenses this file to you under the Apache
   License, Version 2.0 (the "License"); you may not use this file
   except in compliance with the License.  You may obtain a copy of
   the License at

      http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
   implied.  See the License for the specific language governing
   permissions and limitations under the License.

.. default-domain:: cpp

TSHttpTxnInfoIntGet
===================

Synopsis
--------

.. code-block:: cpp

    #include <ts/ts.h>

.. function:: TSReturnCode TSHttpTxnInfoIntGet(TSHttpTxn txnp, TSHttpTxnInfoKey key, TSMgmtInt * value)

Description
-----------

:func:`TSHttpTxnInfoIntGet` returns arbitrary integer-typed info about a transaction as defined in
:type:`TSHttpTxnInfoKey`. The API will be part of a generic API umbrella that can support returning
arbitrary info about a transaction using custom log tags. It works on multiple hooks depending on the
requested info. For example, cache related info may be available only at or after :enumerator:`TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK` hook.

The :type:`TSHttpTxnInfoKey` currently supports the below integer-based info about a transaction

.. enum:: TSHttpTxnInfoKey

   .. enumerator:: TS_TXN_INFO_CACHE_HIT_RAM

      This info is available at or after :enumerator:`TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK` hook. A value of :literal:`1` indicates that the response
      is returned from RAM cache. A value of :literal:`0` indicates otherwise.

   .. enumerator:: TS_TXN_INFO_CACHE_COMPRESSED_IN_RAM

      This info is available at or after :enumerator:`TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK` hook. A value of 1 indicates that the response
      is returned from RAM cache and is compressed. A value of 0 indicates otherwise.

   .. enumerator:: TS_TXN_INFO_CACHE_HIT_RWW

      This info is available at or after :enumerator:`TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK` hook. A value of 1 indicates that the response
      is returned via Read-While-Writer functionality. A value of 0 indicates otherwise.

   .. enumerator:: TS_TXN_INFO_CACHE_OPEN_READ_TRIES

      This info is available at or after :enumerator:`TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK` hook. The value indicates the number of cache open
      read reattempts made by the transaction on cache open read failure.

   .. enumerator:: TS_TXN_INFO_CACHE_OPEN_WRITE_TRIES

      This info is available at or after :enumerator:`TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK` hook. The value indicates the number of cache open
      write reattempts made by the transaction on cache open write failure.

   .. enumerator:: TS_TXN_INFO_CACHE_VOLUME

      This info is available at or after :enumerator:`TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK` hook. The value indicates the cache volume ID used
      for the cache object associated with the transaction.

Return values
-------------

The API returns :enumerator:`TS_SUCCESS`, if the requested info is supported, :enumerator:`TS_ERROR` otherwise.
