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

.. include:: ../../../common.defs

.. default-domain:: cpp

TSMutexUnlock
*************

Synopsis
========

.. code-block:: cpp

    #include <ts/ts.h>

.. function:: void TSMutexUnlock(TSMutex mutexp)

Description
===========

Decrements the recursive lock count.  If the count thus becomes zero, unlocks
the mutex.  This should only be called within the continuation handler function or
:type:`TSThread` function that locked the mutex.  Can also be called within
the continuation handler for the continuation's mutex.  (This is normally done
before the continution handler destroys the continuation running it.)
