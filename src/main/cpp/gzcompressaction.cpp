/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <log4cxx/rolling/gzcompressaction.h>
#include <apr_thread_proc.h>
#include <apr_strings.h>
#include <log4cxx/helpers/exception.h>
#include <log4cxx/helpers/transcoder.h>

using namespace log4cxx;
using namespace log4cxx::rolling;
using namespace log4cxx::helpers;

IMPLEMENT_LOG4CXX_OBJECT(GZCompressAction)

GZCompressAction::GZCompressAction(const File& src,
    const File& dest,
    bool del)
   : source(src), destination(dest), deleteSource(del) {
}

bool GZCompressAction::execute(log4cxx::helpers::Pool& p) const {
    if (source.exists(p)) {
        apr_pool_t* aprpool = p.getAPRPool();
        apr_procattr_t* attr;
        apr_status_t stat = apr_procattr_create(&attr, aprpool);
        if (stat != APR_SUCCESS) throw IOException(stat);
    
        stat = apr_procattr_io_set(attr, APR_NO_PIPE, APR_FULL_BLOCK, APR_FULL_BLOCK);
        if (stat != APR_SUCCESS) throw IOException(stat);
    
        stat = apr_procattr_cmdtype_set(attr, APR_PROGRAM_PATH);
        if (stat != APR_SUCCESS) throw IOException(stat);

        //
        //   set child process output to destination file
        //
        apr_file_t* child_out;
        apr_int32_t flags = APR_FOPEN_READ | APR_FOPEN_WRITE |
            APR_FOPEN_CREATE | APR_FOPEN_TRUNCATE;
        stat = destination.open(&child_out, flags, APR_OS_DEFAULT, p);
        if (stat != APR_SUCCESS) throw IOException(stat);

        stat =  apr_procattr_child_out_set(attr, child_out, NULL);
        if (stat != APR_SUCCESS) throw IOException(stat);

        //
        //   redirect the child's error stream to this processes' error stream
        //
        apr_file_t* child_err;
        stat = apr_file_open_stderr(&child_err, aprpool);
        if (stat == APR_SUCCESS) {
         stat =  apr_procattr_child_err_set(attr, child_err, NULL);
         if (stat != APR_SUCCESS) throw IOException(stat);
        }

        const char** args = (const char**) 
            apr_palloc(aprpool, 4 *sizeof(*args));
        int i = 0;
        args[i++] = "gzip";
        args[i++] = "-c";
        args[i++] = Transcoder::encode(source.getPath(), p);
        args[i++] = NULL;
    

        apr_proc_t pid;
        stat = apr_proc_create(&pid, "gzip", args, NULL, attr, aprpool);
        if (stat != APR_SUCCESS) throw IOException(stat);

        apr_proc_wait(&pid, NULL, NULL, APR_WAIT);
        stat = apr_file_close(child_out);
        if (stat != APR_SUCCESS) throw IOException(stat);
    
        if (deleteSource) {
            source.deleteFile(p);
        }
        return true;
    }
    return false;
}

