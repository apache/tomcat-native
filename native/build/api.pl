#!/usr/bin/perl

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# -----------------------------------------------------------------------------
# Compare the native API with native methods in the Java API.
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# TODO:
# - use method name plus return type plus args as key instead of
#   only method name
# -----------------------------------------------------------------------------
# 

use strict;

# Modules needed, should all be standard

# Commandline option parsing
use Getopt::Std;
# dirname function
use File::Basename;
# Traverse through directory structure and execute
# callback for each file
use File::Find;
# Platform agnostic concatenation of directory and file names
use File::Spec::Functions;

################### BEGIN VARIABLES WHICH MUST BE MAINTAINED #####################

# Script version, printed via getopts with "--version"
our $VERSION = '1.0';

# Sub directories containing c native API and Java classes.
# Path relative to tcnative main project directory.
my $C_NATIVE_SUBDIR = 'native';
my $JAVA_API_SUBDIR = 'java';

# Macro used for marking C native API
my $C_NATIVE_DECLARATION = 'TCN_IMPLEMENT_CALL';

# Macro used for marking C native API arguments
my $C_NATIVE_ARGUMENTS = 'TCN_STDARGS';

################### END VARIABLES WHICH MUST BE MAINTAINED #####################

# Global data variables

# Print verbose output?
my $verbose = 0;

# Signature structures
my %cSignature;
my %javaSignature;


# Usage/Help
sub HELP_MESSAGE {
    my $fh = shift;
    print $fh "Usage:: $0 [ -h ] [ -v ] [ -d DIRECTORY ]\n";
    print $fh "           -h: Print help\n";
    print $fh "           -v: Verbose output\n";
    print $fh " -d DIRECTORY: Path to tcnative project main directory.\n";
    print $fh "               Default is '..\\..' relative to the script directory \n";
}


# Parse arguments:
# -h: help
# -v: verbose
# -d: tcnative project main directory

$Getopt::Std::STANDARD_HELP_VERSION = 1;
our ($opt_h, $opt_v, $opt_d);
getopts('hvd:');

if ($opt_h) {
    HELP_MESSAGE(*STDOUT);
    exit 1;
}

if ($opt_v) {
    $verbose = 1;
}

my $directory = $opt_d;
if (!defined($directory) || $directory eq '') {
    $directory = dirname($0);
    $directory = catfile($directory, ('..', '..'));
}

if (! -d $directory) {
    print STDERR "Invalid tcnative project directory '$directory' - Aborting!\n";
    exit 1;
}

sub mapTypes {
    my $types = shift;
    # Replace "Array" by "[]"
    $types =~ s/Array/\\[\\]/g;
    # Remove leading "j" from scalar types
    $types =~ s/j(byte|short|int|long|boolean)/$1/g;
    # Replace string type
    $types =~ s/jstring/String/g;
    # Replace object type with pattern
    $types =~ s/jobject/([A-Z][A-Za-z0-9]*|[a-z]+\\[\\])/g;
    return $types;
}

# Extract C native API from one file
sub cNativeApi {

    # Only check C source files
    return if $_ !~ /\.c$/;

    my $file = $_;
    my ($m, $signature, $type, $args, $ret, $class, $method);

    print "Checking C API file $file\n" if $verbose;
    open(IN, "<$file") or do {
        print "ERROR: Ignoring file '$file', could not open file for read: $!\n";
        return;
    };

    while(<IN>) {

        # Example declaration:
        #TCN_IMPLEMENT_CALL(jlong, Socket, create)(TCN_STDARGS, jint family,
        #                                          jint type, jint protocol,
        #                                          jlong pool)
        if ($_ =~ /^\s*$C_NATIVE_DECLARATION\s*/o) {
            chomp();
            $signature = $_;
            # Concat next line until signature is complete
            while($signature !~ /\([^\)]*\)\s*\([^\)]*\)/ && $signature !~ /;/ && ($_ = <IN>)) {
                chomp();
                $signature .= $_;
            }

            # Strip C-style comments. See: "perldoc -q comment"
            $signature =~ s#/\*[^*]*\*+([^/*][^*]*\*+)*/|("(\\.|[^"\\])*"|'(\\.|[^'\\])*'|.[^/"'\\]*)#defined $2 ? $2 : ""#gse;

            # Save one declaration
            if ($signature =~ /^\s*$C_NATIVE_DECLARATION\s*\(([^\)]*)\)\s*\(([^\)]*)\)/) {
                ($type, $args) = ($1, $2);

                # Normalize return type and method name
                # Collapse multiple spaces
                $type =~ s/\s+/ /g;
                # Remove spaces around commas
                $type =~ s/, /,/g;
                $type =~ s/ ,/,/g;
                # Trim spaces at start and end
                $type =~ s/^ //;
                $type =~ s/ $//;

                ($ret, $class, $method) = split(/,/, $type);

                # Map return type
                $ret = mapTypes($ret);

                # Normalize argument list
                # Collapse multiple spaces
                $args =~ s/\s+/ /g;
                # Remove spaces around commas
                $args =~ s/, /,/g;
                $args =~ s/ ,/,/g;
                # Remove argument names, only types are needed
                $args =~ s/ [^, ]+//g;
                # Trim spaces at start and end
                $args =~ s/^ //;
                $args =~ s/ $//;
                # Remove leading JNI arguments macro
                $args =~ s/^$C_NATIVE_ARGUMENTS,?//o;

                # Map argument list types
                $args = mapTypes($args);

                $m = $cSignature{$class};
                if (!defined($m)) {
                    $m = $cSignature{$class} = {};
                }
                if (exists($m->{$method}) && ($m->{$method}->{return} ne $ret || $m->{$method}->{args} ne $args)) {
                    print "ERROR: C native file $file method $method in class $class will be overwritten!";
                    print "\tOld signature: '$m->{$method}->{return} $m->{$method}->{method}($m->{$method}->{args})\n";
                    print "\tNew signature: '$ret $method($args)\n";
                }
                # XXX Use method plus ret plus args as key instead
                $m = $m->{$method} = {};
                $m->{method} = $method;
                $m->{return} = $ret;
                $m->{args} = $args;
                print "\tFound C API in $file class $class: '$m->{return} $m->{method}($m->{args})'\n" if $verbose;
            } else {
                print "ERROR: Incomplete C signature in file $file ignored in '$signature'";
            }
        }
    }

    close(IN);
}

# Extract native methods in Java API from one file
sub javaNativeApi {

    # Only check Java source files
    return if $_ !~ /\.java$/;

    my $file = $_;
    my ($m, $signature, $type, $args, $ret, $class, $method);

    print "Checking Java API file $file\n" if $verbose;
    open(IN, "<$_") or do {
        print "ERROR: Ignoring file '$file', could not open file for read: $!\n";
        return;
    };

    $class = $file;
    $class =~ s/\.java$//;

    while(<IN>) {

        # Example declaration
        #public static native long uid(String username, long p)
        #       throws Error;
        if ($_ =~ /^\s*((public|protected|private)\s+)?([^\(]*)\(/ && ($type = $3) && $type =~ /\snative\s/) {
            chomp();
            $signature = $_;
            # Concat next line until signature is complete
            while($signature !~ /\([^\)]*\)/ && $signature !~ /;/ && ($_ = <IN>)) {
                chomp();
                $signature .= $_;
            }

            # Save one declaration
            if ($signature =~ /^\s*([^\(]+)\(([^\)]*)\)/) {
                ($type, $args) = ($1, $2);

                # Normalize return type and method name
                # Remove unused specifiers
                $type =~ s/\b(public|protected|private)\b//g;
                $type =~ s/\bnative\b//g;
                $type =~ s/\bstatic\b//g;
                # Collapse multiple spaces
                $type =~ s/\s+/ /g;
                # Trim spaces at start and end
                $type =~ s/^ //;
                $type =~ s/ $//;

                ($ret, $method) = split(/\s+/, $type);

                # Normalize argument list
                # Collapse multiple spaces
                $args =~ s/\s+/ /g;
                # Remove spaces around commas
                $args =~ s/, /,/g;
                $args =~ s/ ,/,/g;
                # Remove spaces in front of array "[]"
                $args =~ s/ \[\]/[]/g;
                # Remove argument names, only types are needed
                $args =~ s/ [^, ]+//g;
                # Trim spaces at start and end
                $args =~ s/^ //;
                $args =~ s/ $//;

                $m = $javaSignature{$class};
                if (!defined($m)) {
                    $m = $javaSignature{$class} = {};
                }
                if (exists($m->{$method})) {
                    print "ERROR: Java file $file method $method in class $class will be overwritten!";
                    print "\tOld signature: '$m->{$method}->{return} $m->{$method}->{method}($m->{$method}->{args})\n";
                    print "\tNew signature: '$ret $method($args)\n";
                }
                # XXX Use method plus ret plus args as key instead
                $m = $m->{$method} = {};
                $m->{method} = $method;
                $m->{return} = $ret;
                $m->{args} = $args;
                print "\tFound native Java API in $file class $class: '$m->{return} $m->{method}($m->{args})'\n" if $verbose;
            } else {
                print "ERROR: Incomplete Java signature in file $file ignored in '$signature'";
            }
        }
    }

    close(IN);
}

# Search for C native API
find(\&cNativeApi, catfile($directory, $C_NATIVE_SUBDIR));
# Search for native methods in Java API
find(\&javaNativeApi, catfile($directory, $JAVA_API_SUBDIR));

print "Comparing C and Java APIs...\n";

# Check whether all native methods have correct Java counterparts
for my $class (sort keys %cSignature) {
    my $c = $cSignature{$class};
    my $j = $javaSignature{$class};
    if (!defined($j)) {
        print "ERROR: No Java API defined for class '$class'\n";
        for my $method (sort keys %{$c}) {
            my $m = $c->{$method};
            print "\tMissing Java method: $m->{return} $m->{method}($m->{args})\n";
        }
    } else {
        for my $method (sort keys %{$c}) {
            my $m = $c->{$method};
            if (!exists($j->{$method})) {
                print "ERROR: Missing Java method in class '$class': $m->{return} $m->{method}($m->{args})\n";
            } else {
                my $n = $j->{$method};
                if ($n->{return} !~ /^$m->{return}$/ || $n->{args} !~ /^$m->{args}$/) {
                    print "ERROR: Incompatible API in class '$class'!\n";
                    print "\tC signature: $m->{return} $m->{method}($m->{args})\n";
                    print "\tJava signature: $n->{return} $n->{method}($n->{args})\n";
                }
                delete($j->{$method});
            }
        }
    }
}

# Check whether any native Java API methods remain
for my $class (sort keys %javaSignature) {
    my $j = $javaSignature{$class};
    my $c = $cSignature{$class};
    if (!defined($c)) {
        print "ERROR: No C API defined for class '$class'!\n";
        for my $method (sort keys %{$j}) {
            my $m = $j->{$method};
            print "\tMissing C method: $m->{return} $m->{method}($m->{args})\n";
        }
    } else {
        for my $method (sort keys %{$j}) {
            my $m = $j->{$method};
            if (!exists($c->{$method})) {
                print "ERROR: Missing C method in class '$class': $m->{return} $m->{method}($m->{args})\n";
            } else {
                my $n = $c->{$method};
                if ($m->{return} !~ /^$n->{return}$/ || $m->{args} !~ /^$n->{args}$/) {
                    print "ERROR: Incompatible API in class '$class'!\n";
                    print "\tC signature: $n->{return} $n->{method}($n->{args})\n";
                    print "\tJava signature: $m->{return} $m->{method}($m->{args})\n";
                }
                delete($c->{$method});
            }
        }
    }
}

print "Done.\n";
