/*
 * Copyright (c) 2009 NLNet Labs. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <libxml/parser.h>
#include <CUnit/Basic.h>
#include <CUnit/Automated.h>
#include "janitor.h"
#include "locks.h"
#include "parser/confparser.h"
#include "daemon/engine.h"
#include "daemon/signercommands.h"

static char* argv0;
static char* workdir;
static engine_type* engine;
static janitor_threadclass_t debugthreadclass;
static janitor_thread_t debugthread;

static void
initialize(int argc, char* argv[])
{
    /* this initialization should happen only once */
    if (argv[0][0] != '/') {
        char *path = getcwd(NULL, 0);
        asprintf(&argv0, "%s/%s", path, argv[0]);
        free(path);
    } else {
        argv0 = strdup(argv[0]);
    }

    if (argc > 1) {
        workdir = argv[1];
    }

    ods_janitor_initialize(argv0);

    /* initialize */
    xmlInitGlobals();
    xmlInitParser();
    xmlInitThreads();

    janitor_threadclass_create(&debugthreadclass, "debug");
    janitor_threadclass_setautorun(debugthreadclass);
}

static void
enginerunner(void* engine)
{
    engine_start(engine);
}

static int
setUp(void)
{
    int linkfd, status;

    ods_log_init("test", 0, NULL, 3);
    
    if (workdir != NULL)
        chdir(workdir);
    
    engine = engine_create();
    if((status = engine_setup_config(engine, "conf.xml", 3, 0)) != ODS_STATUS_OK ||
       (status = engine_setup_initialize(engine, &linkfd)) != ODS_STATUS_OK ||
       (status = engine_setup_start(engine)) != ODS_STATUS_OK ||
       (status = engine_setup_finish(engine, linkfd)) != ODS_STATUS_OK) {
        ods_log_error("Unable to start signer daemon: %s", ods_status2str(status));
    }

    janitor_thread_create(&debugthread, debugthreadclass, enginerunner, engine);

    return (status == ODS_STATUS_OK);
}



static int
tearDown(void)
{
    command_stop(engine);
    janitor_thread_join(debugthread);

    ods_log_close();
    return 0;
}

static void
finalize(void)
{
    janitor_threadclass_destroy(debugthreadclass);
    xmlCleanupParser();
    xmlCleanupGlobals();
    free(argv0);
}

void
testBasic(void)
{
    /* no-op */
}

int
main(int argc, char* argv[])
{
    CU_pSuite pSuite = NULL;

    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* add a suite to the registry */
    if (!(pSuite = CU_add_suite("Signer", setUp, tearDown))) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    if (!(CU_add_test(pSuite, "test of start stop", testBasic)) ||
            !(CU_add_test(pSuite, "test of start stop2", testBasic))) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    initialize(argc, argv);

#ifdef NOTDEFINED
    CU_set_output_filename(const char* szFilenameRoot)
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
#endif
    CU_list_tests_to_file();
    CU_automated_run_tests();
    CU_cleanup_registry();

    finalize();

    return CU_get_error();
}