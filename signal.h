#ifndef __SIGNAL_H__
#define __SIGNAL_H__

/*
 * Copyright 2013 Marco Lizza (marco.lizza@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */

/* minimum required definitions for Lua */

#define SIG_ERR	((__sighandler_t) -1)		/* Error return.  */
#define SIG_DFL	((__sighandler_t) 0)		/* Default action.  */
#define SIG_IGN	((__sighandler_t) 1)		/* Ignore signal.  */
#define	SIGINT		2	/* Interrupt (ANSI).  */

typedef void (*__sighandler_t) (int);

void (*signal(int sig, void (*func)(int)))(int);

typedef int sig_atomic_t;

#endif  /* __SIGNAL_H__ */