%%% Copyright (c) 2013, Michael Santos <michael.santos@gmail.com>
%%%
%%% Permission to use, copy, modify, and/or distribute this software for any
%%% purpose with or without fee is hereby granted, provided that the above
%%% copyright notice and this permission notice appear in all copies.
%%%
%%% THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
%%% WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
%%% MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
%%% ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
%%% WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
%%% ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
%%% OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
-module(liblxc_tests).

-compile(export_all).

-include_lib("eunit/include/eunit.hrl").

% lxc_container_clear_config/1
% lxc_container_set_config_item/3
% lxc_container_load_config/2
% lxc_container_init_pid/1
% argv/1

erlxc_test_() ->
    {setup,
        fun start/0,
        fun stop/1,
        fun run/1
    }.

run(Ref) ->
    [
        list(Ref, active),
        list(Ref, all),
        list(Ref, defined),
        name(Ref),
        config_file_name(Ref),
        ?MODULE:get_keys(Ref),
        get_config_item(Ref),
        clear_config_item(Ref),
        defined(Ref),
        running(Ref),
        get_config_path(Ref),
        set_config_path(Ref)
    ].

start() ->
    {ok, Ref} = erlxc_drv:start([{name, <<"erlxc">>}]),
    Ref.

stop(Ref) ->
    erlxc_drv:stop(Ref).

list(Ref, Type) ->
    Reply = liblxc:list(Ref, Type),
    ?_assertMatch({ok, _}, Reply).

name(Ref) ->
    Reply = liblxc:name(Ref),
    ?_assertEqual(<<"erlxc">>, Reply).

config_file_name(Ref) ->
    Reply = liblxc:config_file_name(Ref),
    ?_assertEqual(true, is_binary(Reply)).

get_keys(Ref) ->
    {ok, Bin} = liblxc:get_keys(Ref),
    Keys = binary:split(Bin, <<"\n">>, [global,trim]),
    Exists = lists:member(<<"lxc.utsname">>, Keys),
    ?_assertEqual(true, Exists).

get_config_item(Ref) ->
    Reply = liblxc:get_config_item(Ref, <<"lxc.utsname">>),
    ?_assertMatch({ok, <<"erlxc">>}, Reply).

clear_config_item(Ref) ->
    {ok, _} = liblxc:get_config_item(Ref, <<"lxc.network">>),
    ok = liblxc:clear_config_item(Ref, <<"lxc.network">>),
    ok = liblxc:clear_config_item(Ref, <<"lxc.network">>),
    Reply = liblxc:get_config_item(Ref, <<"lxc.network">>),
    ?_assertEqual({error,none}, Reply).

defined(Ref) ->
    Reply = liblxc:defined(Ref),
    ?_assertEqual(true, Reply).

running(Ref) ->
    Reply = liblxc:running(Ref),
    ?_assertEqual(false, Reply).

get_config_path(Ref) ->
    Reply = liblxc:get_config_path(Ref),
    ?_assertEqual(true, is_binary(Reply)).

set_config_path(Ref) ->
    Path = liblxc:get_config_path(Ref),
    Reply = liblxc:set_config_path(Ref, Path),
    ?_assertEqual(ok, Reply).
