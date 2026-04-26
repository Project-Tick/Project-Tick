/*
 * Copyright (c) 2017-2025, Feral Interactive and the GameMode contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Feral Interactive nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * ---
 *
 * GameMode client header — vendored from:
 *   https://github.com/FeralInteractive/gamemode/blob/master/lib/gamemode_client.h
 *
 * GameMode supports the following client functions.
 * Requests are refcounted in the daemon.
 *
 *   int gamemode_request_start()         — Request gamemode starts; 0=success, -1=fail
 *   int gamemode_request_end()           — Request gamemode ends;   0=success, -1=fail
 *   int gamemode_query_status()          — 0=inactive, 1=active, 2=active+registered, -1=error
 *   int gamemode_request_start_for(pid_t pid) — Request gamemode for another process
 *   int gamemode_request_end_for(pid_t pid)   — End   gamemode for another process
 *   int gamemode_query_status_for(pid_t pid)  — Query gamemode for another process
 *   const char *gamemode_error_string()  — Last error description string
 *
 * GAMEMODE_AUTO can be defined before including to auto-request/release via
 * constructor/destructor attributes. Errors are then printed to stderr.
 *
 * Loads libgamemode dynamically at runtime so that clients can be shipped
 * without a hard dependency on gamemode being installed.
 */

#ifndef CLIENT_GAMEMODE_H
#define CLIENT_GAMEMODE_H

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

static char internal_gamemode_client_error_string[512] = { 0 };

/*
 * Load libgamemode dynamically so that clients remain independent of whether
 * gamemode is installed on the host.
 */
static volatile int internal_libgamemode_loaded = 1; /* 1=not yet, 0=ok, -1=fail */

/* Function pointer typedefs */
typedef int (*api_call_return_int)(void);
typedef const char *(*api_call_return_cstring)(void);
typedef int (*api_call_pid_return_int)(pid_t);

/* Storage for resolved function pointers */
static api_call_return_int    REAL_internal_gamemode_request_start     = NULL;
static api_call_return_int    REAL_internal_gamemode_request_end       = NULL;
static api_call_return_int    REAL_internal_gamemode_query_status      = NULL;
static api_call_return_int    REAL_internal_gamemode_request_restart   = NULL;
static api_call_return_cstring REAL_internal_gamemode_error_string     = NULL;
static api_call_pid_return_int REAL_internal_gamemode_request_start_for = NULL;
static api_call_pid_return_int REAL_internal_gamemode_request_end_for   = NULL;
static api_call_pid_return_int REAL_internal_gamemode_query_status_for  = NULL;

/* Internal: safely bind a dlsym symbol. Returns 0 on success, -1 on failure. */
__attribute__((always_inline)) static inline int
internal_bind_libgamemode_symbol(void *handle, const char *name, void **out_func,
                                 size_t func_size, bool required)
{
	void *symbol_lookup = NULL;
	char *dl_error = NULL;

	symbol_lookup = dlsym(handle, name);
	dl_error = dlerror();
	if (required && (dl_error || !symbol_lookup)) {
		snprintf(internal_gamemode_client_error_string,
		         sizeof(internal_gamemode_client_error_string),
		         "dlsym failed - %s", dl_error);
		return -1;
	}
	memcpy(out_func, &symbol_lookup, func_size);
	return 0;
}

/* Internal: load libgamemode. Returns 0 on success, -1 on failure. */
__attribute__((always_inline)) static inline int internal_load_libgamemode(void)
{
	if (internal_libgamemode_loaded != 1)
		return internal_libgamemode_loaded;

	struct binding {
		const char *name;
		void **functor;
		size_t func_size;
		bool required;
	} bindings[] = {
		{ "real_gamemode_request_start",
		  (void **)&REAL_internal_gamemode_request_start,
		  sizeof(REAL_internal_gamemode_request_start), true },
		{ "real_gamemode_request_end",
		  (void **)&REAL_internal_gamemode_request_end,
		  sizeof(REAL_internal_gamemode_request_end), true },
		{ "real_gamemode_query_status",
		  (void **)&REAL_internal_gamemode_query_status,
		  sizeof(REAL_internal_gamemode_query_status), false },
		{ "real_gamemode_request_restart",
		  (void **)&REAL_internal_gamemode_request_restart,
		  sizeof(REAL_internal_gamemode_request_restart), false },
		{ "real_gamemode_error_string",
		  (void **)&REAL_internal_gamemode_error_string,
		  sizeof(REAL_internal_gamemode_error_string), true },
		{ "real_gamemode_request_start_for",
		  (void **)&REAL_internal_gamemode_request_start_for,
		  sizeof(REAL_internal_gamemode_request_start_for), false },
		{ "real_gamemode_request_end_for",
		  (void **)&REAL_internal_gamemode_request_end_for,
		  sizeof(REAL_internal_gamemode_request_end_for), false },
		{ "real_gamemode_query_status_for",
		  (void **)&REAL_internal_gamemode_query_status_for,
		  sizeof(REAL_internal_gamemode_query_status_for), false },
	};

	void *libgamemode = dlopen("libgamemode.so.0", RTLD_NOW);
	if (!libgamemode) {
		libgamemode = dlopen("libgamemode.so", RTLD_NOW);
		if (!libgamemode) {
			snprintf(internal_gamemode_client_error_string,
			         sizeof(internal_gamemode_client_error_string),
			         "dlopen failed - %s", dlerror());
			internal_libgamemode_loaded = -1;
			return -1;
		}
	}

	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
		struct binding *b = &bindings[i];
		if (internal_bind_libgamemode_symbol(libgamemode, b->name, b->functor,
		                                     b->func_size, b->required)) {
			internal_libgamemode_loaded = -1;
			return -1;
		}
	}

	internal_libgamemode_loaded = 0;
	return 0;
}

/* Returns the last error string. */
__attribute__((always_inline)) static inline const char *gamemode_error_string(void)
{
	if (internal_load_libgamemode() < 0 ||
	    internal_gamemode_client_error_string[0] != '\0') {
		return internal_gamemode_client_error_string;
	}
	assert(REAL_internal_gamemode_error_string != NULL);
	return REAL_internal_gamemode_error_string();
}

/* Request gamemode to start (for this process). */
#ifdef GAMEMODE_AUTO
__attribute__((constructor))
#else
__attribute__((always_inline)) static inline
#endif
int gamemode_request_start(void)
{
	if (internal_load_libgamemode() < 0) {
#ifdef GAMEMODE_AUTO
		fprintf(stderr, "gamemodeauto: %s\n", gamemode_error_string());
#endif
		return -1;
	}
	assert(REAL_internal_gamemode_request_start != NULL);
	if (REAL_internal_gamemode_request_start() < 0) {
#ifdef GAMEMODE_AUTO
		fprintf(stderr, "gamemodeauto: %s\n", gamemode_error_string());
#endif
		return -1;
	}
	return 0;
}

/* Request gamemode to end (for this process). */
#ifdef GAMEMODE_AUTO
__attribute__((destructor))
#else
__attribute__((always_inline)) static inline
#endif
int gamemode_request_end(void)
{
	if (internal_load_libgamemode() < 0) {
#ifdef GAMEMODE_AUTO
		fprintf(stderr, "gamemodeauto: %s\n", gamemode_error_string());
#endif
		return -1;
	}
	assert(REAL_internal_gamemode_request_end != NULL);
	if (REAL_internal_gamemode_request_end() < 0) {
#ifdef GAMEMODE_AUTO
		fprintf(stderr, "gamemodeauto: %s\n", gamemode_error_string());
#endif
		return -1;
	}
	return 0;
}

/* Query gamemode status. 0=inactive, 1=active, 2=active+registered, -1=error. */
__attribute__((always_inline)) static inline int gamemode_query_status(void)
{
	if (internal_load_libgamemode() < 0)
		return -1;
	if (REAL_internal_gamemode_query_status == NULL) {
		snprintf(internal_gamemode_client_error_string,
		         sizeof(internal_gamemode_client_error_string),
		         "gamemode_query_status missing (older host?)");
		return -1;
	}
	return REAL_internal_gamemode_query_status();
}

/* Request gamemode for another process by PID. 0=ok, -1=fail, -2=rejected. */
__attribute__((always_inline)) static inline int
gamemode_request_start_for(pid_t pid)
{
	if (internal_load_libgamemode() < 0)
		return -1;
	if (REAL_internal_gamemode_request_start_for == NULL) {
		snprintf(internal_gamemode_client_error_string,
		         sizeof(internal_gamemode_client_error_string),
		         "gamemode_request_start_for missing (older host?)");
		return -1;
	}
	return REAL_internal_gamemode_request_start_for(pid);
}

/* End gamemode for another process by PID. 0=ok, -1=fail, -2=rejected. */
__attribute__((always_inline)) static inline int
gamemode_request_end_for(pid_t pid)
{
	if (internal_load_libgamemode() < 0)
		return -1;
	if (REAL_internal_gamemode_request_end_for == NULL) {
		snprintf(internal_gamemode_client_error_string,
		         sizeof(internal_gamemode_client_error_string),
		         "gamemode_request_end_for missing (older host?)");
		return -1;
	}
	return REAL_internal_gamemode_request_end_for(pid);
}

/* Query gamemode status for another process. 0=inactive, 1=active, 2=active+this, -1=error. */
__attribute__((always_inline)) static inline int
gamemode_query_status_for(pid_t pid)
{
	if (internal_load_libgamemode() < 0)
		return -1;
	if (REAL_internal_gamemode_query_status_for == NULL) {
		snprintf(internal_gamemode_client_error_string,
		         sizeof(internal_gamemode_client_error_string),
		         "gamemode_query_status_for missing (older host?)");
		return -1;
	}
	return REAL_internal_gamemode_query_status_for(pid);
}

#endif /* CLIENT_GAMEMODE_H */
