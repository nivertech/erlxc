/* Copyright (c) 2013, Michael Santos <michael.santos@gmail.com>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "erlxc.h"
#include "erlxc_cmd.h"

static char **erlxc_list_to_argv(ETERM *);
static void erlxc_free_argv(char **);
static ETERM *erlxc_list_containers(erlxc_state_t *, ETERM *,
        int (*)(const char *, char ***, struct lxc_container ***));

/* Options */
enum {
    ERLXC_TERMINATE_ON_EXIT = 1 << 0,   /* Destroy the container when the port stops */
};

    ETERM *
erlxc_cmd(erlxc_state_t *ep, u_int32_t cmd, ETERM *arg)
{
    erlxc_cmd_t *fun = NULL;

    VERBOSE(2, "cmd=%d", cmd);
    if (ep->verbose >= 2)
        erl_print_term(stderr, arg);

    if (cmd >= sizeof(cmds)/sizeof(cmds[0]))
        return erlxc_errno(EINVAL);

    fun = &cmds[cmd];

    if (!ERL_IS_LIST(arg) || erl_length(arg) != fun->narg)
        return erl_mk_atom("badarg");

    return (*fun->fp)(ep, arg);
}

    static ETERM *
erlxc_lxc_container_name(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;

    if (!c)
        return erlxc_errno(EINVAL);

    return (c->name ? erl_mk_binary(c->name, strlen(c->name)) : erl_mk_binary("", 0));
}

    static ETERM *
erlxc_lxc_container_defined(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;

    if (!c)
        return erlxc_errno(EINVAL);

    return (c->is_defined(c) ? erl_mk_atom("true") : erl_mk_atom("false"));
}

    static ETERM *
erlxc_lxc_container_running(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;

    if (!c)
        return erlxc_errno(EINVAL);

    return (c->is_running(c) ? erl_mk_atom("true") : erl_mk_atom("false"));
}

    static ETERM *
erlxc_lxc_container_create(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    char *t = NULL;
    char *bdevtype = NULL;
    struct bdev_specs *specs = NULL;
    int flags = 0;
    char **argv = NULL;
    int fdout = -1;
    int fdnull = -1;
    bool res;

    if (!c)
        return erlxc_errno(EINVAL);

    /* template */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0)
        t = erl_iolist_to_string(hd);

    if (!t)
        goto BADARG;

    /* bdevtype */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    /* specs */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    /* flags */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    flags = ERL_INT_VALUE(hd);

    /* argv */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (!ERL_IS_EMPTY_LIST(hd)) {
        argv = erlxc_list_to_argv(hd);
        if (!argv)
            goto BADARG;
    }

    (void)fflush(stdout);
    fdout = dup(STDOUT_FILENO);
    fdnull = open("/dev/null", O_WRONLY);
    (void)dup2(fdnull, STDOUT_FILENO);
    (void)close(fdnull);

    res = c->create(
            c,
            t,
            bdevtype,
            specs,
            flags,
            argv
            );

    (void)fflush(stdout);
    (void)dup2(fdout, STDOUT_FILENO);
    (void)close(fdout);

    erlxc_free_argv(argv);

    return (res ? erl_mk_atom("true") : erl_mk_atom("false"));

BADARG:
    erlxc_free_argv(argv);
    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_destroy(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;

    if (!c)
        return erlxc_errno(EINVAL);

    return (c->destroy(c) ? erl_mk_atom("true") : erl_mk_atom("false"));
}

    static ETERM *
erlxc_lxc_container_start(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    int useinit = 0;
    char **argv = NULL;
    bool res;
    int errnum = 0;
    pid_t pid = -1;

    if (!c)
        return erlxc_errno(EINVAL);

    /* useinit */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    useinit = ERL_INT_VALUE(hd);

    /* argv */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (!ERL_IS_EMPTY_LIST(hd)) {
        argv = erlxc_list_to_argv(hd);
        if (!argv)
            goto BADARG;
    }

    if (!lxc_container_get(c))
        goto BADARG;

    pid = fork();

    switch (pid) {
        case -1:
            return erlxc_errno(errnum);
        case 0:
            res = c->start(c, useinit, argv);
            (void)res;
            erl_err_quit("container stopped");
        default:
            erlxc_free_argv(argv);
            return erlxc_tuple2(erl_mk_atom("ok"), erl_mk_int(pid));
    }

BADARG:
    erlxc_free_argv(argv);
    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_stop(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;
    bool res;
    int errnum = 0;

    if (!c)
        return erlxc_errno(EINVAL);

    errno = 0;
    res = c->stop(c);
    errnum = errno;

    lxc_container_put(c);
    return (res ? erl_mk_atom("ok") : erlxc_errno(errnum));
}

    static ETERM *
erlxc_lxc_container_shutdown(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    int timeout = -1;

    if (!c)
        return erlxc_errno(EINVAL);

    /* timeout */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    timeout = ERL_INT_VALUE(hd);
    if (timeout < 0)
        goto BADARG;

    return (c->shutdown(c, timeout) ? erl_mk_atom("true") : erl_mk_atom("false"));

BADARG:
    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_init_pid(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;
    pid_t pid = -1;

    if (!c)
        return erlxc_errno(EINVAL);

    pid = c->init_pid(c);

    /* XXX overflow pid_t -> int */
    return erlxc_tuple2(erl_mk_atom("ok"), erl_mk_int(pid));
}

    static ETERM *
erlxc_lxc_container_load_config(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    char *path = NULL;

    if (!c)
        return erlxc_errno(EINVAL);

    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0) {
        path = erl_iolist_to_string(hd);
        if (!path)
            goto BADARG;
    }

    return (c->load_config(c, path) ? erl_mk_atom("ok") : erlxc_errno(errno));

BADARG:
    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_get_keys(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    char *key = NULL;
    char buf[2048] = {0};
    int n = 0;
    int len = 0;

    if (!c)
        return erlxc_errno(EINVAL);

    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0) {
        key = erl_iolist_to_string(hd);
        if (!key)
            goto BADARG;
    }

    if (!key)
        len = c->get_keys(c, NULL, NULL, 0);

    /* XXX better error if length is too large */
    if (len < 0 || len >= sizeof(buf))
        goto BADARG;

    n = c->get_keys(c, key, buf, (key ? sizeof(buf) : len+1));

    return (n > 0 ? erlxc_tuple2(erl_mk_atom("ok"), erl_mk_binary(buf, n)) : erlxc_error("none"));

BADARG:
    erl_free(key);

    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_config_file_name(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;
    char *name = NULL;

    if (!c)
        return erlxc_errno(EINVAL);

    name = c->config_file_name(c);
    if (!name)
        return erl_mk_binary("",0);

    free(name);

    return erl_mk_binary(name, strlen(name));
}

    static ETERM *
erlxc_lxc_container_clear_config(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;

    if (!c)
        return erlxc_errno(EINVAL);

    c->clear_config(c);
    return erl_mk_atom("ok");
}

    static ETERM *
erlxc_lxc_container_clear_config_item(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    char *key = NULL;
    bool res;
    int errnum = 0;

    if (!c)
        return erlxc_errno(EINVAL);

    /* key */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0)
        key = erl_iolist_to_string(hd);

    if (!key)
        goto BADARG;

    res = c->clear_config_item(c, key);
    errnum = errno;

    erl_free(key);

    return (res ? erl_mk_atom("ok") : erlxc_errno(errnum));

BADARG:
    erl_free(key);

    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_get_config_item(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    char *key = NULL;
    char *buf = NULL;
    int n = 0;
    ETERM *res = NULL;

    if (!c)
        return erlxc_errno(EINVAL);

    /* key */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0)
        key = erl_iolist_to_string(hd);

    if (!key)
        goto BADARG;

    n = c->get_config_item(c, key, NULL, 0);

    /* 0 is ??? */
    if (n < 1) {
        erl_free(key);
        return erlxc_error("none");
    }

    /* account for null */
    buf = erl_malloc(n+1);
    if (!buf)
        goto BADARG;

    (void)c->get_config_item(c, key, buf, n+1);

    /* null is not included in binary */
    res = erl_mk_binary(buf, n);

    erl_free(key);
    erl_free(buf);

    return erlxc_tuple2(erl_mk_atom("ok"), res);

BADARG:
    erl_free(key);
    erl_free(buf);

    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_set_config_item(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    char *key = NULL;
    char *val = NULL;
    bool res;
    int errnum = 0;

    if (!c)
        return erlxc_errno(EINVAL);

    /* key */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0)
        key = erl_iolist_to_string(hd);

    if (!key)
        goto BADARG;

    /* value */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0) {
        val = erl_iolist_to_string(hd);
        if (!val)
            goto BADARG;
    }

    errno = 0;
    res = c->set_config_item(c, key, val);
    errnum = errno;

    erl_free(key);
    erl_free(val);

    return (res ? erl_mk_atom("ok") : erlxc_errno(errnum));

BADARG:
    erl_free(key);
    erl_free(val);

    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_lxc_container_get_config_path(erlxc_state_t *ep, ETERM *arg)
{
    struct lxc_container *c = ep->c;
    const char *path = NULL;

    if (!c)
        return erlxc_errno(EINVAL);

    path = c->get_config_path(c);

    if (!path)
        return erl_mk_binary("",0);

    return erl_mk_binary(path, strlen(path));
}

    static ETERM *
erlxc_lxc_container_set_config_path(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    struct lxc_container *c = ep->c;
    char *path = NULL;

    if (!c)
        return erlxc_errno(EINVAL);

    /* path */
    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0)
        path = erl_iolist_to_string(hd);

    if (!path)
        goto BADARG;

    if (c->set_config_path(c, path)) {
        erl_free(path);
        return erl_mk_atom("ok");
    }

BADARG:
    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_list_active_containers(erlxc_state_t *ep, ETERM *arg)
{
    return erlxc_list_containers(ep, arg, list_active_containers);
}

    static ETERM *
erlxc_list_all_containers(erlxc_state_t *ep, ETERM *arg)
{
    return erlxc_list_containers(ep, arg, list_all_containers);
}

    static ETERM *
erlxc_list_defined_containers(erlxc_state_t *ep, ETERM *arg)
{
    return erlxc_list_containers(ep, arg, list_defined_containers);
}

    static ETERM *
erlxc_list_containers(erlxc_state_t *ep, ETERM *arg,
        int (*fun)(const char *path, char ***names, struct lxc_container ***cr))
{
    int n = -1;
    int i = 0;
    ETERM *hd = NULL;
    char *path = NULL;
    char **names = NULL;
    ETERM **reply = NULL;
    int errnum = 0;

    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        goto BADARG;

    if (erl_iolist_length(hd) > 0) {
        path = erl_iolist_to_string(hd);
        if (!path)
            goto BADARG;
    }

    errno = 0;
    n = fun(path, &names, NULL);
    errnum = errno;

    //erl_free_term(arg1);
    erl_free(path);

    if (n < 0)
        return erlxc_errno(errnum);

    reply = erl_malloc(n * sizeof(ETERM **));
    for (i = 0; i < n; i++) {
        reply[i] = erl_mk_binary(names[i], strnlen(names[i], MAXHOSTNAMELEN));
        free(names[i]);
    }

    if (n > 0)
        free(names);

    return erlxc_ok(erl_mk_list(reply, n));

BADARG:
    return erl_mk_atom("badarg");
}

    static ETERM *
erlxc_argv(erlxc_state_t *ep, ETERM *arg)
{
    ETERM *hd = NULL;
    char **argv = NULL;
    int len = 0;
    int i = 0;

    arg = erlxc_list_head(&hd, arg);
    if (!hd)
        return erl_mk_atom("badarg");

    len = erl_length(hd);
    VERBOSE(0, "len=%d", len);

    argv = erlxc_list_to_argv(hd);

    if (!argv)
        return erl_mk_atom("badarg");

    for (i = 0; i < len; i++)
        (void)fprintf(stderr, "arg[%d]=%s", i, argv[i]);

    return erl_mk_atom("ok");
}

    char **
erlxc_list_to_argv(ETERM *arg)
{
    ETERM *hd = NULL;
    int len = 0; /* XXX overflow */
    int i = 0;
    char **argv = NULL;

    len = erl_length(arg);

    /* NULL terminate */
    argv = calloc(len + 1, sizeof(char **));

    if (!argv)
        return NULL;

    for (i = 0; i < len; i++) {
        arg = erlxc_list_head(&hd, arg);
        if (!hd)
            return NULL;

        argv[i] = erl_iolist_to_string(hd);
        if (!argv[i])
            return NULL;
    }

    return argv;
}

    static void
erlxc_free_argv(char **argv)
{
    int i = 0;

    if (argv == NULL)
        return;

    for (i = 0; argv[i] != NULL; i++)
        erl_free(argv[i]);
}
