# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * 4 * blocks();

#no_long_string();

run_tests();

#no_diff();

__DATA__

=== TEST 1: basic convertion
--- config
    location /foo {
        tee html/bar.txt;
        echo $arg_msg;
    }

# a root location is already provided by test framework
#    location / {
#        root html;
#    }
--- request eval
["GET /foo?msg=Hello", "GET /bar.txt"]
--- response_body eval
["Hello
","Hello
"]
