#!/usr/local/bin/perl

# =========================================================================
#
#                 The Apache Software License,  Version 1.1
#
#          Copyright (c) 1999-2003 The Apache Software Foundation.
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

# Author: Glenn Nielsen

# Script for generating reports and graphs using statistical data generated
# by the tomcat_trend.pl script.
#
# The following graphs are created:
#
#  tomcat_request.png
#    Long term trend graph of total number of tomcat requests handled
#
#  tomcat_median.png
#    Long term overall trend graph of tomcat request latency median
#
#  tomcat_deviation.png
#    Long term overall trend graph of tomcat request mean and standard deviation
#
#  tomcat_error.png
#    Long term trend graph of requests rejected by tomcat. Shows requests rejected
#    when tomcat has no request processors available.  Can be an indicator that tomcat
#    is overloaded or having other scaling problems.
#
#  tomcat_client.png
#    Long term trend graph of requests forward to tomcat which were aborted by the remote
#    client (browser).  You will normally see some aborted requests.  High numbers of these
#    can be an indicator that tomcat is overloaded or there are requests which have very high
#    latency.
#
# tomcat_reports.pl <directory where statistics are archived> <directory to place graphs/reports in>

use GD;
use GD::Graph;
use GD::Graph::Data;
use GD::Graph::lines;
use GD::Graph::linespoints;
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

$archivedir = $ARGV[0];
$reportdir = $ARGV[1];

die "Usage: $0 archivedir reportdir"
   unless( length($archivedir) && ($reportdir) );

die "Archive Directory $archivedir doesn't exist"
   unless( -d $archivedir);

die "Report Directory $reportdir doesn't exist"
   unless( -d $reportdir);

# Read in data from file
die "Archive Directory $archivedir has no global.data file"
   unless( -e "$archivedir/global.data" );

@Data = `tail -365 $archivedir/global.data`;
$numdays = $#Data;
$daycounter = $numdays;

foreach( @Data ) {
   $line = $_;
   chomp($line);
   ($date,$count,$median,$mean,$stddev,$min,$max,$client_gone,$tomcat_full) = split /\s+/,$line;
   # print "$daycounter $date $count $median $mean $stdev $min $max $client_gone $tomcat_full\n";
   $start_time = $date unless $start_time>0;
   $end_time = $date;
   push @days,int($daycounter/7);
   push @count,$count;
   push @median,$median;
   push @mean,$mean;
   push @stddev,$mean+$stddev;
   push @min,$min;
   push @max,$max;
   push @client_gone,$client_gone;
   push @tomcat_full,$tomcat_full;
   $daycounter--;
}

($day,$mon,$year) = (localtime($start_time))[3..5];
$year += 1900;
$startdate = "$Months[$mon] $day, $year";
($day,$mon,$year) = (localtime($end_time))[3..5];
$year += 1900;
$enddate = "$Months[$mon] $day, $year";

# Output request trend graph
$outfile = "$reportdir/tomcat_request.png";
unlink $outfile;

$stats = Statistics::Descriptive::Sparse->new();
$stats->add_data(@count);
$max = $stats->max();
$min = $stats->min();

&RequestGraph();

# Output median latency trend graph
$outfile = "$reportdir/tomcat_median.png";
unlink $outfile;

$stats = Statistics::Descriptive::Sparse->new();
$stats->add_data(@median);
$max = $stats->max();
$min = $stats->min();

&MedianGraph();

# Output latency deviation trend graph
$outfile = "$reportdir/tomcat_deviation.png";
unlink $outfile;

$stats = Statistics::Descriptive::Sparse->new();
$stats->add_data(@stddev);
$stats->add_data(@mean);
$max = $stats->max();
$min = $stats->min();

&DeviationGraph();

# Output request error trend graph
$outfile = "$reportdir/tomcat_error.png";
unlink $outfile;

$stats = Statistics::Descriptive::Sparse->new();
$stats->add_data(@tomcat_full);
$max = $stats->max();
$min = $stats->min();

&ErrorGraph();

# Output request error trend graph
$outfile = "$reportdir/tomcat_client.png";
unlink $outfile;

$stats = Statistics::Descriptive::Sparse->new();
$stats->add_data(@client_gone);
$max = $stats->max();
$min = $stats->min();

&ClientGraph();

exit;

sub RequestGraph {

  $graph = GD::Graph::lines->new(800,600);
  @data = (\@days,\@count);

  $div = 100;
  $div = 500 if $max >= 2000;
  $div = 1000 if $max >= 5000;
  $div = 5000 if $max >= 20000;
  $div = 10000 if $max >= 50000;
  $div = 50000 if $max >= 200000;
  $div = 100000 if $max >= 500000;
  $div = 500000 if $max >= 2000000;
  $div = 1000000 if $max >= 5000000;
  $ymax = (int($max/$div) + 1)*$div;
  $ymin = int($min/$div)*$div;
  $ytick = ($ymax - $ymin)/$div;

  $graph->set(
    y_label             => 'Requests',
    title               => "Tomcat Requests by Day from $startdate to $enddate",
    y_min_value         => $ymin,
    y_max_value         => $ymax,
    y_tick_number       => $ytick,
    y_number_format     => \&val_format,
    x_label		=> 'Weeks Ago',
    x_label_skip	=> 7,
    x_tick_offset	=> $numdays%7,
    dclrs               => [ qw(green) ],
    legend_placement    => 'BC'
  ) or warn $graph->error;

  $graph->set_legend( 'Requests' );
  $graph->set_title_font(GD::gdGiantFont);
  $graph->set_x_axis_font(GD::gdSmallFont);
  $graph->set_y_axis_font(GD::gdSmallFont);
  $graph->set_legend_font(GD::gdSmallFont);
  $gd = $graph->plot(\@data);
  die "\$gd not defined" unless defined $gd;

  open IMG, ">$outfile" or die $!;
  print IMG $gd->png or die $gd->error;
  close IMG;
}

sub MedianGraph {

  $graph = GD::Graph::lines->new(800,600);
  @data = (\@days,\@median);

  $div = .05;
  $div = .1 if $max >= .5;
  $div = .5 if $max >= 2;
  $div = 1 if $max >= 5;
  $div = 5 if $max >= 20;
  $div = 10 if $max >= 50;
  $div = 50 if $max >= 200;
  $div = 100 if $max >= 500;
  $ymax = (int($max/$div) + 1)*$div;
  $ytick = $ymax/$div;

  $graph->set(
    y_label             => 'Latency (Seconds)',
    title               => "Tomcat Request Median Latency by Day from $startdate to $enddate",
    y_min_value		=> 0,
    y_max_value         => $ymax,
    y_tick_number       => $ytick,
    y_number_format     => \&val_format,
    x_label             => 'Weeks Ago',
    x_label_skip        => 7,
    x_tick_offset       => $numdays%7,
    dclrs               => [ qw(green) ],
    legend_placement    => 'BC'
  ) or warn $graph->error;

  $graph->set_legend( 'Median' );
  $graph->set_title_font(GD::gdGiantFont);
  $graph->set_x_axis_font(GD::gdSmallFont);
  $graph->set_y_axis_font(GD::gdSmallFont);
  $graph->set_legend_font(GD::gdSmallFont);
  $gd = $graph->plot(\@data);
  die "\$gd not defined" unless defined $gd;

  open IMG, ">$outfile" or die $!;
  print IMG $gd->png or die $gd->error;
  close IMG;
}

sub DeviationGraph {

  $graph = GD::Graph::lines->new(800,600);
  @data = (\@days,\@mean,\@stddev);

  $div = .1;
  $div = .5 if $max >= 2;
  $div = 1 if $max >= 5;
  $div = 5 if $max >= 20;
  $div = 10 if $max >= 50;
  $div = 50 if $max >= 200;
  $div = 100 if $max >= 500;
  $ymax = (int($max/$div) + 1)*$div;
  $ytick = $ymax/$div;

  $graph->set(
    y_label             => 'Latency (Seconds)',
    title               => "Tomcat Request Latency Mean and Deviation by Day from $startdate to $enddate",
    y_max_value         => $ymax,
    y_tick_number       => $ytick,
    x_label             => 'Weeks Ago',
    x_label_skip        => 7,
    x_tick_offset       => $numdays%7,
    dclrs               => [ qw(green yellow) ],
    legend_placement    => 'BC'
  ) or warn $graph->error;

  $graph->set_legend( 'Mean', 'Mean plus Standard Deviation' );
  $graph->set_title_font(GD::gdGiantFont);
  $graph->set_x_axis_font(GD::gdSmallFont);
  $graph->set_y_axis_font(GD::gdSmallFont);
  $graph->set_legend_font(GD::gdSmallFont);
  $gd = $graph->plot(\@data);
  die "\$gd not defined" unless defined $gd;

  open IMG, ">$outfile" or die $!;
  print IMG $gd->png or die $gd->error;
  close IMG;
}

sub ErrorGraph {

  $graph = GD::Graph::lines->new(800,600);
  @data = (\@days,\@tomcat_full);

  $div = 5;
  $div = 10 if $max >=100;
  $div = 50 if $max >= 200;
  $div = 100 if $max >= 1000;
  $div = 500 if $max >= 2000;
  $div = 1000 if $max >= 5000;
  $div = 5000 if $max >= 20000;
  $div = 10000 if $max >= 50000;
  $div = 50000 if $max >= 200000;
  $div = 100000 if $max >= 500000;
  $div = 500000 if $max >= 2000000;
  $div = 1000000 if $max >= 5000000;
  $ymax = (int($max/$div) + 1)*$div;
  $ymin = int($min/$div)*$div;
  $ytick = ($ymax - $ymin)/$div;

  $graph->set(
    y_label             => 'Requests',
    title               => "Tomcat Rejected Request by Day from $startdate to $enddate",
    y_min_value         => $ymin,
    y_max_value         => $ymax,
    y_tick_number       => $ytick,
    y_number_format     => \&val_format,
    x_label             => 'Weeks Ago',
    x_label_skip        => 7,
    x_tick_offset       => $numdays%7,
    dclrs               => [ qw(green) ],
    legend_placement    => 'BC'
  ) or warn $graph->error;

  $graph->set_legend( 'Tomcat Rejected Requests' );
  $graph->set_title_font(GD::gdGiantFont);
  $graph->set_x_axis_font(GD::gdSmallFont);
  $graph->set_y_axis_font(GD::gdSmallFont);
  $graph->set_legend_font(GD::gdSmallFont);
  $gd = $graph->plot(\@data);
  die "\$gd not defined" unless defined $gd;

  open IMG, ">$outfile" or die $!;
  print IMG $gd->png or die $gd->error;
  close IMG;
}

sub ClientGraph {

  $graph = GD::Graph::lines->new(800,600);
  @data = (\@days,\@client_gone);

  $div = 5;
  $div = 10 if $max >=100;
  $div = 50 if $max >= 200;
  $div = 100 if $max >= 1000;
  $div = 500 if $max >= 2000;
  $div = 1000 if $max >= 5000;
  $div = 5000 if $max >= 20000;
  $div = 10000 if $max >= 50000;
  $div = 50000 if $max >= 200000;
  $div = 100000 if $max >= 500000;
  $div = 500000 if $max >= 2000000;
  $div = 1000000 if $max >= 5000000;
  $ymax = (int($max/$div) + 1)*$div;
  $ymin = int($min/$div)*$div;
  $ytick = ($ymax - $ymin)/$div;

  $graph->set(
    y_label             => 'Requests',
    title               => "Tomcat Client Aborted Requests by Day from $startdate to $enddate",
    y_min_value         => $ymin,
    y_max_value         => $ymax,
    y_tick_number       => $ytick,
    y_number_format     => \&val_format,
    x_label             => 'Weeks Ago',
    x_label_skip        => 7,
    x_tick_offset       => $numdays%7,
    dclrs               => [ qw(green) ],
    legend_placement    => 'BC'
  ) or warn $graph->error;

  $graph->set_legend( 'Tomcat Client Aborted Requests' );
  $graph->set_title_font(GD::gdGiantFont);
  $graph->set_x_axis_font(GD::gdSmallFont);
  $graph->set_y_axis_font(GD::gdSmallFont);
  $graph->set_legend_font(GD::gdSmallFont);
  $gd = $graph->plot(\@data);
  die "\$gd not defined" unless defined $gd;

  open IMG, ">$outfile" or die $!;
  print IMG $gd->png or die $gd->error;
  close IMG;
}

sub val_format {
  my $value = shift;
  my $ret;

  $ret = $value;
  if( $ret =~ /\./ ) {
    $ret =~ s/\.(\d\d\d).*/\.$1/;
  } else {
    $ret =~ s/(\d+)(\d\d\d)$/$1,$2/;
    $ret =~ s/(\d+)(\d\d\d),(\d\d\d)$/$1,$2,$3/;
  }
  return $ret;
}

sub size_format {
  my $value = shift;
  my $ret;

  if( $max >= 5000 ) {
     $value = int(($value/1024)+.5);
  }
  $ret = $value;
  $ret =~ s/(\d+)(\d\d\d)$/$1,$2/;
  $ret =~ s/(\d+)(\d\d\d),(\d\d\d)$/$1,$2,$3/;
  return $ret;
}
