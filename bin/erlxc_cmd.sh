#!/bin/sh

cat<< EOF
/* Copyright (c) 2013-$(date +%Y), Michael Santos <michael.santos@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
EOF

PROTO=$1

OIFS=$IFS
while read line; do
    IFS=/
    set -- $line
    printf "static ETERM *erlxc_%s(erlxc_state_t *, ETERM *);\n" $1
done < $PROTO

cat<< 'EOF'

/* commands */
typedef struct {
    ETERM *(*fp)(erlxc_state_t *, ETERM *);
    u_int8_t narg;
} erlxc_cmd_t;

erlxc_cmd_t cmds[] = {
EOF

while read line; do
    IFS=/
    set -- $line
    printf "    {erlxc_%s, %s},\n" $1 $2
done < $PROTO

cat<< 'EOF'
};
EOF
