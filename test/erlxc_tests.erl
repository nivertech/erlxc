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
-module(erlxc_tests).

-compile(export_all).

-include_lib("eunit/include/eunit.hrl").
-include_lib("erlxc/include/erlxc.hrl").

erlxc_test_() ->
    {setup,
        fun startit/0,
        fun stopit/1,
        fun runit/1
    }.

runit(Container) ->
    [
        erlxc_exit(Container)
    ].

startit() ->
    Verbose = list_to_integer(getenv("ERLXC_TEST_VERBOSE", "0")),
    Bridge = getenv("ERLXC_TEST_BRIDGE", <<"lxcbr0">>),

    % XXX this fails if ERLXC_TEST_BRIDGE=br0
%    Config = [{config, [
%            {<<"lxc.network.type">>, <<"veth">>},
%            {<<"lxc.network.link">>, Bridge},
%            {<<"lxc.network.flags">>, <<"up">>}
%        ]}],

    % XXX Generates the same config as above except adds duplicate
    % XXX lxc.network.link items, one with lxcbr0 and the other set to br0
    Config = [
        {verbose, Verbose},
        {config, [
            {<<"lxc.network.link">>, Bridge}
        ]}
    ],

    erlxc:spawn(<<>>, Config).

stopit(Container) ->
    erlxc_drv:stop(Container#container.port).

erlxc_exit(Container) ->
    true = erlxc:exit(Container, kill),
    Reply = liblxc:running(Container#container.port),
    ?_assertEqual(false, Reply).

getenv(Var, Default) ->
    case os:getenv(Var) of
        false -> Default;
        N -> N
    end.
