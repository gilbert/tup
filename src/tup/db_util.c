/* vim: set ts=8 sw=8 sts=8 noet tw=78:
 *
 * tup - A file-based build system
 *
 * Copyright (C) 2009-2011  Mike Shal <marfey@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "db_util.h"
#include <stdio.h>

int db_close(sqlite3 *db, sqlite3_stmt **stmts, int num)
{
	int x;
	for(x=0; x<num; x++) {
		if(stmts[x])
			sqlite3_finalize(stmts[x]);
	}

	if(sqlite3_close(db) != 0) {
		fprintf(stderr, "Unable to close database: %s\n",
			sqlite3_errmsg(db));
		return -1;
	}
	return 0;
}
