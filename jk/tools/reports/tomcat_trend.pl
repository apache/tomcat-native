#!/usr/local/bin/perl

# ========================================================================= 
#                                                                           
#                 The Apache Software License,  Version 1.1                 
#                                                                           
#          Copyright (c) 1999-2001 The Apache Software Foundation.          
#                           All rights reserved.                            
#                                                                           
# ========================================================================= 
#                                                                           
# Redistribution and use in source and binary forms,  with or without modi- 
# fication, are permitted provided that the following conditions are met:   
#                                                                           
# 1. Redistributions of source code  must retain the above copyright notice 
#    notice, this list of conditions and the following disclaimer.          
#                                                                           
# 2. Redistributions  in binary  form  must  reproduce the  above copyright 
#    notice,  this list of conditions  and the following  disclaimer in the 
#    documentation and/or other materials provided with the distribution.   
#                                                                           
# 3. The end-user documentation  included with the redistribution,  if any, 
#    must include the following acknowlegement:                             
#                                                                           
#       "This product includes  software developed  by the Apache  Software 
#        Foundation <http://www.apache.org/>."                              
#                                                                           
#    Alternately, this acknowlegement may appear in the software itself, if 
#    and wherever such third-party acknowlegements normally appear.         
#                                                                           
# 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     
#    Foundation"  must not be used  to endorse or promote  products derived 
#    from this  software without  prior  written  permission.  For  written 
#    permission, please contact <apache@apache.org>.                        
#                                                                           
# 5. Products derived from this software may not be called "Apache" nor may 
#    "Apache" appear in their names without prior written permission of the 
#    Apache Software Foundation.                                            
#                                                                           
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES 
# INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY 
# AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL 
# THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY 
# DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL 
# DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS 
# OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) 
# HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, 
# STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
# ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE 
# POSSIBILITY OF SUCH DAMAGE.                                               
#                                                                           
# ========================================================================= 
#                                                                           
# This software  consists of voluntary  contributions made  by many indivi- 
# duals on behalf of the  Apache Software Foundation.  For more information 
# on the Apache Software Foundation, please see <http://www.apache.org/>.   
#                                                                           
# =========================================================================

# $Header$
# $Revision$
# $Date$

# Author:  Glenn Nielsen

# Script for analyzing mod_jk.log data when logging tomcat request data using
# the JkRequestLogFormat Apache mod_jk configuration.
#
# Generates statistics for request latency and errors.  Archives the generated
# data to files for later use in long term trend graphs and reports.
#
# tomcat_trend.pl <directory containing mod_jk.log> <directory for archiving statistics>

use FileHandle;
use Statistics::Descriptive;
use Time::Local;

# Constants

%MON = ('JAN' => 0, 'Jan' => 0,
        'FEB' => 1, 'Feb' => 1,
        'MAR' => 2, 'Mar' => 2,
        'APR' => 3, 'Apr' => 3,
        'MAY' => 4, 'May' => 4,
        'JUN' => 5, 'Jun' => 5,
        'JUL' => 6, 'Jul' => 6,
        'AUG' => 7, 'Aug' => 7,
        'SEP' => 8, 'Sep' => 8,
        'OCT' => 9, 'Oct' => 9,
        'NOV' => 10, 'Nov' => 10,
        'DEC' => 11, 'Dec' => 11,);

@Months = ("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");

# Check the args

$logdir= $ARGV[0];
$archivedir = $ARGV[1];

die "Usage: $0 logdir archivedir"
   unless( length($logdir) && length($archivedir) );

die "Log Directory $logdir doesn't exist"
   unless( -d $logdir);

die "Archive Directory $archivedir doesn't exist"
   unless( -d $archivedir);

# Get start date from global.data if it exists

if( -e "$archivedir/global.data" ) {
   # Get the start date from the last entry in global.data
   # print "Checking global.data for startdate\n";
   @tail = `tail -1 $archivedir/global.data`;
   $startdate = (split /\s+/,$tail[0])[0];
   ($day, $mon, $year) = (localtime($startdate))[3..5];
   $startdate = timelocal(0,0,0,$day+1,$mon,$year);

}

($day, $mon, $year) = (localtime(time))[3..5];
$curdate = timelocal(0,0,0,$day,$mon,$year);
print "Today: " . scalar(localtime($curdate)) . "\n";

# Get the log files names and date they start
@logs = `ls -1 $logdir/mod_jk.log*`;
foreach( @logs ) {
   $logfile = $_;
   chomp($logfile);
   next if ( $logfile =~ /\.(bz2|gz|zip)$/ );
   @head = `head -1 $logfile`;
   ($mon, $day, $time, $year) = (split /\s+/,$head[0])[1..4];
   ($hour, $min, $sec) = split /:/,$time;
   $year =~ s/\]$//;
   # print "$head[0]\n";
   # print "$mon $day $time $year $hour $min $sec\n";
   $logtime = timelocal($sec,$min,$hour,$day,$MON{$mon},$year-1900);
   # print "$logfile $logtime " . scalar(localtime($logtime)) . "\n";
   $modjklog{$logtime} = $logfile;
}

# Set the startdate if this is the first time processing the logs
# If we have a startdate, remove log files we con't need to process
foreach $logtime ( sort {$a <=> $b} keys %modjklog ) {
   # If logs haven't been processed before, set startdate to time of 
   # first log entry
   if( $startdate !~ /^\d+$/ ) {
      $startdate = $logtime;
      ($day, $mon, $year) = (localtime($startdate))[3..5];
      $startdate = timelocal(0,0,0,$day,$mon,$year);
      last;
   }
   if( $logtime > $startdate ) {
      last;
   }
   # Save the previous log file since start date may start here
   $prevlogfile = $modjklog{$logtime};
   $prevlogtime = $logtime;
   # Remove log files we don't need to process
   delete $modjklog{$logtime};
}

# Add back in the previous log file where we need to start processing
if( defined $prevlogtime ) {
   $modjklog{$prevlogtime} = $prevlogfile;
}

print "StartDate: " . scalar(localtime($startdate)) . "\n";

foreach $key ( sort {$a <=> $b} keys %modjklog ) {
   last if( $key >= $curdate );
   $logfile = $modjklog{$key};
   $fh = new FileHandle "<$logfile";  
   die "Open of logfile $logfile failed: $!"
      unless defined $fh;
   print "Processing log: $logfile\n";
   while( $line = $fh->getline) {
      chomp($line);
      ($mon, $day, $time, $year) = (split /\s+/,$line)[1..4];
      ($hour, $min, $sec) = split /:/,$time;
      $year =~ s/\]$//;
      # print "$mon $day $time $year $hour $min $sec\n";
      $logtime = timelocal($sec,$min,$hour,$day,$MON{$mon},$year-1900);

      if( $logtime > $startdate ) {
         $origline = $line;
         # Strip off the leading date and time
         # print "$line\n";
         $line =~ s/^\[.*\] //;
         # print "$line\n";

         # See if this is a new 5 minute period
         $interval = int($logtime/300);
         if( $interval != $previnterval ) {
            if( defined $previnterval ) {
               &IntervalStats(\%Global,\%Interval,$previnterval*300);
            }
            undef %Interval;
            undef @IntervalLatency;
            undef %IntervalWorkers;
            $Interval{tomcat_full} = 0;
            $Interval{client_gone} = 0;
            $Interval{latency} = \@IntervalLatency;
            $Interval{workers} = \%IntervalWorkers;
            $previnterval = $interval;
         }

         # See if this is a new day
         if( $day != $prevday ) {
            if( defined $prevday ) {
               &DailyStats($startdate,\%Global);
            }
            undef %Global;
            undef %GlobalWorkers;
            undef @GlobalLatency;
            $Global{tomcat_full} = 0;
            $Global{client_gone} = 0;
            $Global{interval} = "";
            $Global{latency} = \@GlobalLatency;
            $Global{workers} = \%GlobalWorkers;
            $Global{errors} = "";
            $prevday = $day;
            $startdate = $logtime;
         }

         # Stop processing if logtime is today
         last if( $logtime >= $curdate );

         if( $line =~ /\d\)\]: / ) {
            # Handle a mod_jk error
            # print "mod_jk error! " . scalar(localtime($logtime)) . " $line\n";
            if( $line =~ /(jk_tcp_socket_recvfull failed|ERROR: Receiving from tomcat failed)/ ) {
               $Global{tomcat_full}++;
               $Interval{tomcat_full}++;
            } elsif( $line =~ /(ajp_process_callback - write failed|ERROR sending data to client. Connection aborted or network problems|Client connection aborted or network problems)/ ) {
               $Global{client_gone}++;
               $Interval{client_gone}++;
            }
            next;
         } else {
            # Handle a mod_jk request log entry
            # print "$line\n";
            $line =~ s/^\[.*\] //;
            # print "$line\n";
            $line =~ s/\"(GET|POST|OPTIONS|HEAD)[^\"]*\" //;
            # print "$line\n";
            $line =~ s/[\?\;].*\"//;
            # print "$line\n";
            $line =~ s/\"//g;
            # print "$line\n";
            ($work, $host, $page, $status, $latency) = split /\s+/,$line;
            $page =~ s/\/\//\//g;
            $page =~ s/\.\//\//g;
            # print scalar(localtime($logtime)) . " $work $host $page $status $latency\n";
            if( length($work) <= 0 || length($host) <= 0 ||
                length($page) <= 0 || $status !~ /^\d+$/ || $latency !~ /^\d+\.\d+$/ ) {
               print "Unknown log entry: $origline\n" unless $origline =~ /\.c /;
               next;
            }

            # Throw out abnormally long requests and log them as an error
            if( $latency >= 1800 ) {
               $Global{errors} .= "Error: $page has an HTTP status of $status and an ";
               $Global{errors} .= "abnormally long request latency of $latency seconds\n";
               next;
            }

            # Save the data by day for Global, Worker, and Host
            push @{$Global{latency}},$latency;
            $workers = $Global{workers};
            if( !defined $$workers{$work} ) {
               undef @{"$work"};
               undef %{"$work"};
               undef %{"$work-hosts"};
               ${"$work"}{latency} = \@{"$work"};
               ${"$work"}{hosts} = \%{"$work-hosts"};
               ${"$work"}{interval} = "";
               $$workers{$work} = \%{"$work"};
            }
            $worker = $$workers{$work};
            push @{$$worker{latency}},$latency;

            if( !defined $$worker{hosts}{$host} ) {
               undef @{"$work-$host"};
               undef %{"$work-$host"};
               undef %{"$work-$host-pages"};
               ${"$work-$host"}{latency} = \@{"$work-$host"};
               ${"$work-$host"}{pages} = \%{"$work-$host-pages"};
               ${"$work-$host"}{interval} = "";
               $$worker{hosts}{$host} = \%{"$work-$host"};
            }
            $hoster = $$worker{hosts}{$host};
            push @{$$hoster{latency}},$latency;

            if( !defined $$hoster{pages}{$page} ) {
                undef @{"$work-$host-$page"};
                $$hoster{pages}{$page} = \@{"$work-$host-$page"};
            }
            push @{$$hoster{pages}{$page}},$latency;

            # Save the data by 5 minute interval for Global, Worker, and Host
            push @{$Interval{latency}},$latency;
            $workers = $Interval{workers};
            if( !defined $$workers{"$work"} ) {
               undef @{"int-$work"};
               undef %{"int-$work"};
               undef %{"int-$work-hosts"};
               ${"int-$work"}{latency} = \@{"int-$work"};
               ${"int-$work"}{hosts} = \%{"int-$work-hosts"};
               $$workers{$work} = \%{"int-$work"};
            }
            $worker = $$workers{$work};
            push @{$$worker{latency}},$latency;

            if( !defined $$worker{hosts}{$host} ) {
               undef @{"int-$work-$host"};
               undef %{"int-$work-$host"};
               ${"int-$work-$host"}{latency} = \@{"int-$work-$host"};
               $$worker{hosts}{$host} = \%{"int-$work-$host"};
            }
            $hoster = $$worker{hosts}{$host};
            push @{$$hoster{latency}},$latency;

         }
      }
   }
   undef $fh;
}

# If the last log file ends before switch to the current day,
# output the last days data
if( $logtime < $curdate ) {
   &IntervalStats(\%Global,\%Interval,$previnterval*300);
   &DailyStats($startdate,\%Global);
}

exit;

sub IntervalStats($$$) {
   my $global = $_[0];
   my $data = $_[1];
   my $interval = $_[2];

   ($count,$median,$mean,$stddev,$min,$max) = &CalcStats($$data{latency});
   $$global{interval} .= "$interval $count $median $mean $stddev $min $max $$data{client_gone} $$data{tomcat_full}\n";

   foreach $work ( keys %{$$data{workers}} ) {
      $worker = $$data{workers}{$work};
      $gworker = $$global{workers}{$work};
      ($count,$median,$mean,$stddev,$min,$max) = &CalcStats($$worker{latency});
      $$gworker{interval} .= "$interval $count $median $mean $stddev $min $max\n";
      foreach $host ( keys %{$$worker{hosts}} ) {
         $hoster = $$worker{hosts}{$host};
         $ghoster = $$gworker{hosts}{$host};
         ($count,$median,$mean,$stddev,$min,$max) = &CalcStats($$hoster{latency});
         $$ghoster{interval} .= "$interval $count $median $mean $stddev $min $max\n";
      }
   }
}

sub DailyStats($$) {
   my $date = $_[0];
   my $data = $_[1];

   &SaveStats($data,$date,"","global");
   &SaveFile($$data{interval},$date,"","daily");
   foreach $work ( keys %{$$data{workers}} ) {
      $worker = $$data{workers}{$work};
      &SaveStats($worker,$date,$work,"global");
      &SaveFile($$worker{interval},$date,$work,"daily");
      foreach $host ( keys %{$$worker{hosts}} ) {
         $hoster = $$worker{hosts}{$host};
         &SaveStats($hoster,$date,"$work/$host","global");
         &SaveFile($$hoster{interval},$date,"$work/$host","daily");
         $pagedata = "";
         foreach $page ( sort keys %{$$hoster{pages}} ) {
            $pager = $$hoster{pages}{$page};
            ($count,$median,$mean,$stddev,$min,$max) = &CalcStats($pager);
            $pagedata .= "$page $count $median $mean $stddev $min $max\n";
         }
         $pagedata .= $$data{errors};
         &SaveFile($pagedata,$date,"$work/$host","request");
      }
   }
}

sub CalcStats($) {
   my $data = $_[0];

   $stats = Statistics::Descriptive::Full->new();
   $stats->add_data(@{$data});
   $median = $stats->median();
   $mean = $stats->mean();
   $stddev = $stats->standard_deviation();
   $max = $stats->max();
   $min = $stats->min();
   $count = $stats->count();
   return ($count,$median,$mean,$stddev,$min,$max);
}

sub SaveStats($$$$) {
   my $data = $_[0];
   my $date = $_[1];
   my $dir = $_[2];
   my $file = $_[3];

   if( length($dir) > 0 ) {
      $dir = "$archivedir/$dir";
   } else {
      $dir = $archivedir;
   }
   mkdir "$dir",0755;

   $outfile = "$dir/${file}.data";

   ($count,$median,$mean,$stddev,$min,$max) = &CalcStats($$data{latency});

   open DATA, ">>$outfile" or die $!;
   print DATA "$date $count $median $mean $stddev $min $max";
   print DATA " $$data{client_gone} $$data{tomcat_full}" if defined $$data{tomcat_full};
   print DATA "\n";
   close DATA;
}

sub SaveFile($$$$) {
   my $data = $_[0];
   my $date = $_[1];
   my $dir = $_[2];
   my $file = $_[3];
   my ($day, $mon, $year);

   ($day, $mon, $year) = (localtime($date))[3..5];
   $year += 1900;
   $mon++;
   $mon = "0$mon" if $mon < 10;
   $day = "0$day" if $day < 10;
   $file = "$year-$mon-$day-$file";

   if( length($dir) > 0 ) {
      $dir = "$archivedir/$dir";
   } else {
      $dir = $archivedir;
   }
   mkdir "$dir",0755;

   $outfile = "$dir/${file}.data";

   open DATA, ">>$outfile" or die $!;
   print DATA $data;
   close DATA;
}
