<<
# Scandoc template file.
#
# This is an example set of templates that is designed to create several
# different kinds of index files. It generates a "master index" which intended
# for use with a frames browser; A "package index" which is the root page of
# the index, and then "package files" containing documentation for all of the
# classes within a single package.

###############################################################################
## Defaults for look and feel

if ($project == "") { $project = "WebApp Library"; }
if ($copyright == "") { $copyright = "2001, The Apache Software Foundation"; }
if ($body_bgcolor == "") { $body_bgcolor = '#ffffff'; }
if ($body_text    == "") { $body_text    = '#000000'; }
if ($body_link    == "") { $body_link    = '#0000ff'; }
if ($body_vlink   == "") { $body_vlink   = '#0000ff'; }
if ($body_alink   == "") { $body_alink   = '#0000ff'; }

###############################################################################
# Generate the frameset index                                                 #
###############################################################################

file "index.html";
@p = packages();
$_ = @p[0]->url;
s/\s/_/g;
y/[A-Z]/[a-z]/;
>>
<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; iso-8859-1">
    <title>$project API Documentation</title>
  </head>
  <frameset cols="200,*">
    <frameset rows="150,*">
      <frame src="packages.html">
      <frame src="pkg.$_" name="pkg">
    </frameset>
    <frame src="doc.$_" name="doc">
  </frameset>
</html>
<<

###############################################################################
# Generate the packages list                                                  #
###############################################################################

file "packages.html";
>>
<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; iso-8859-1">
    <title>$project - Packages List</title>
  </head>
  <body link="$body_link" vlink="$body_vlink" alink="$body_alink"
        bgcolor="$body_bgcolor" text="$body_text">
    <font size="+1" face="arial,helvetica,sans serif">
      <nobr><b>$project packages:</b></nobr>
    </font>
    <table border="0" cellspacing="0" cellpadding="0">
      <tr>
        <td width="10">&nbsp;</td>
        <td>&nbsp;</td>
      </tr>
<<

## For each package, generate an index entry.

foreach $p (packages()) {
    $_ = $p->url;
    s/\s/_/g;
    y/[A-Z]/[a-z]/;
    >>
          <tr>
            <td width="10">&nbsp;</td>
            <td>
              <font face="arial,helvetica,sans serif">
                <nobr><a href="pkg.$_" target="pkg">$(p.name)</a></nobr>
              </font>
            </td>
          </tr>
    <<
}

>>
    </table>
    <hr>
    <font size="-2" face="arial,helvetica,sans serif">
      <div align="center">
        Copyright &copy; $copyright.<br>
        All Rights Reserved.<br>
      </div>
    </font>
  </body>
</html>
<<

###############################################################################
# Generate the packages table of content for each package                     #
###############################################################################
foreach $p (packages()) {
    $_ = $p->url;
    s/\s/_/g;
    y/[A-Z]/[a-z]/;
    file "pkg.$_";
    >>
    <html>
      <head>
        <meta http-equiv="Content-Type" content="text/html; iso-8859-1">
        <title>$project - Packages List</title>
      </head>

      <body link="$body_link" vlink="$body_vlink" alink="$body_alink"
            bgcolor="$body_bgcolor" text="$body_text">
        <font size="+1" face="arial,helvetica,sans serif">
          <nobr><b><a href="doc.$_" target="doc">$(p.name)</a> package:</b></nobr>
        </font>
        <table border="0" cellspacing="0" cellpadding="0">
          <tr>
            <td width="10">&nbsp;</td>
            <td>&nbsp;</td>
          </tr>
          <tr>
            <td colspan="2">
              <font face="arial,helvetica,sans serif">
                <nobr>Classes:</nobr>
              </font>
            </td>
          </tr>
    <<
    foreach $e ($p->classes()) {
        $_ = $e->url;
        s/\s/_/g;
        y/[A-Z]/[a-z]/;
        >>
              <tr>
                <td width="10">&nbsp;</td>
                <td>
                  <font face="arial,helvetica,sans serif">
                    <nobr><a href="doc.$_" target="doc">$(e.fullname)</a></nobr>
                  </font>
                </td>
              </tr>
        <<
    }

    if ($p->globalfuncs()) {
        >>
              <tr>
                <td colspan="2">
                  <font face="arial,helvetica,sans serif">
                    <nobr>Global Functions:</nobr>
                  </font>
                </td>
              </tr>
        <<
        foreach $e ($p->globalfuncs()) {
            $_ = $e->url;
            s/\s/_/g;
            y/[A-Z]/[a-z]/;
            >>
                  <tr>
                    <td width="10">&nbsp;</td>
                    <td>
                      <font face="arial,helvetica,sans serif">
                        <nobr><a href="doc.$_" target="doc">$(e.fullname)</a></nobr>
                      </font>
                    </td>
                  </tr>
            <<
        }
    } else {
        >>
              <tr>
                <td colspan="2">
                  <font face="arial,helvetica,sans serif">
                    <nobr><i>No Global Functions defined.</i></nobr>
                  </font>
                </td>
              </tr>
        <<
    }

    if ($p->globalvars()) {
        >>
              <tr>
                <td colspan="2">
                  <font face="arial,helvetica,sans serif">
                    <nobr>Global Variables:</nobr>
                  </font>
                </td>
              </tr>
        <<
        foreach $e ($p->globalvars()) {
            $_ = $e->url;
            s/\s/_/g;
            y/[A-Z]/[a-z]/;
            >>
                  <tr>
                    <td width="10">&nbsp;</td>
                    <td>
                      <font face="arial,helvetica,sans serif">
                        <nobr><a href="doc.$_" target="doc">$(e.fullname)</a></nobr>
                      </font>
                    </td>
                  </tr>
            <<
        }
    } else {
        >>
              <tr>
                <td colspan="2">
                  <font face="arial,helvetica,sans serif">
                    <nobr><i>No Global Variables defined.</i></nobr>
                  </font>
                </td>
              </tr>
        <<
    }

    >>
        </table>
        <hr>
        <font size="-2" face="arial,helvetica,sans serif">
          <div align="center">
            Copyright &copy; $copyright.<br>
            All Rights Reserved.<br>
          </div>
        </font>
      </body>
    </html>
    <<
}

###############################################################################
# Generate the packages detailed description for each package                 #
###############################################################################
foreach $p (packages()) {
    $_ = $p->url;
    s/\s/_/g;
    y/[A-Z]/[a-z]/;
    file "doc.$_";
    >>
    <html>
      <head>
        <meta http-equiv="Content-Type" content="text/html; iso-8859-1">
        <title>$project - Packages List</title>
      </head>
      <body link="$body_link" vlink="$body_vlink" alink="$body_alink"
            bgcolor="$body_bgcolor" text="$body_text">
        <table width="100%" cellspacing="0" cellpadding="2" border="1">
          <tr>
            <td bgcolor="ccccff">
              <table border="0" width="100%" cellspacing="0" cellpadding="0">
                <tr>
                  <td bgcolor="ccccff" align="left">
                    <font size="+1" face="arial,helvetica,sans serif">
                      <b>$project</b>
                    </font>
                  </td>
                  <td bgcolor="ccccff" align="right">
                    <font size="+1" face="arial,helvetica,sans serif">
                      <b>$(p.name) package</b>
                    </font>
                  </td>
                </tr>
              </table>
            </td>
          </tr>
        </table>
        <br>
        <table width="100%" cellspacing="0" cellpadding="2" border="1">
          <tr>
            <td bgcolor="eeeeff" align="left">
              <font face="arial,helvetica,sans serif">
                <b>Classes</b>
              </font>
            </td>
          </tr>
    <<
    foreach $e ($p->classes()) {
        $_ = $e->url;
        s/\s/_/g;
        y/[A-Z]/[a-z]/;
        >>
              <tr>
                <td>
                  <dl>
                    <dt>
                      <font face="arial,helvetica,sans serif">
                        <nobr><a href="$_">$(e.fullname)</a></nobr>
                      </font>
                    </dt>
        <<
        if ($e->members()) {
            foreach $m ($e->members()) {
                $_ = $m->url;
                s/\s/_/g;
                y/[A-Z]/[a-z]/;
                >>
                            <dd>
                              <font size="-1" face="arial,helvetica,sans serif">
                                <nobr><a href="$_">$(m.fullname)</a></nobr>
                              </font>
                            </dd>
                <<
            }
        }
        >>
                  </dl>
                </td>
              </tr>
        <<
    }

    if ($p->globalfuncs()) {
        >>
            </table>
            <br>
            <table width="100%" cellspacing="0" cellpadding="2" border="1">
              <tr>
                <td bgcolor="eeeeff" align="left">
                  <font face="arial,helvetica,sans serif">
                    <b>Global Functions</b>
                  </font>
                </td>
              </tr>
        <<
        foreach $e ($p->globalfuncs()) {
            $_ = $e->url;
            s/\s/_/g;
            y/[A-Z]/[a-z]/;
            >>
                  <tr>
                    <td>
                      <font face="arial,helvetica,sans serif">
                        <nobr><a href="$_">$(e.fullname)</a></nobr>
                      </font>
                    </td>
                  </tr>
            <<
        }
    }

    if ($p->globalvars()) {
        >>
            </table>
            <br>
            <table width="100%" cellspacing="0" cellpadding="2" border="1">
              <tr>
                <td bgcolor="eeeeff" align="left">
                  <font face="arial,helvetica,sans serif">
                    <b>Global Variables</b>
                  </font>
                </td>
              </tr>
        <<
        foreach $e ($p->globalvars()) {
            $_ = $e->url;
            s/\s/_/g;
            y/[A-Z]/[a-z]/;
            >>
                  <tr>
                    <td>
                      <font face="arial,helvetica,sans serif">
                        <nobr><a href="$_">$(e.fullname)</a></nobr>
                      </font>
                    </td>
                  </tr>
            <<
        }
    }

    >>
        </table>
        <br>
    <<




    if ($p->globalfuncs()) {
        >>
            <table width="100%" cellspacing="0" cellpadding="2" border="1">
              <tr>
                <td bgcolor="ccccff" align="left">
                  <font size="+1" face="arial,helvetica,sans serif">
                    <b>Global Functions Detail:</b>
                  </font>
                </td>
              </tr>
            </table>
        <<
        foreach $e ($p->globalfuncs()) {
            $_ = $e->url;
            s/\s/_/g;
            y/[A-Z]/[a-z]/;
            >>
                <font size="+1" face="arial,helvetica,sans serif">
                  <b><a name="$(e.name)">$(e.name)</a></b>
                </font>
                <dl>
                  <dt><code>$(e.fullname)</code></dt>
                  <dd>
                    <dl>
                      <dt>$(e.description)</dt>
            <<
            if ($e->params()) {
                >>
                          <dt><b>Parameters</b></dt>
                <<
                foreach $a ($e->params()) {
                >>
                          <dd>
                            <code>$(a.name)</code>
                            $(a.description)
                          </dd>
                <<
                }
            }
            if ($e->returnValue()) {
                >>
                          <dt><b>Return Value</b></dt>
                          <dd>$(e.returnValue)</dd>
                <<
            }
            if ($e->exceptions()) {
                >>
                          <dt><b>Exceptions</b></dt>
                <<
                foreach $a ($e->exceptions()) {
                >>
                          <dd>
                            <code>$(a.name)</code>
                            $(a.description)
                          </dd>
                <<
                }
            }
            >>
                    </dl>
                  </dd>
                </dl>
                <hr>
            <<
        }
    }
    >>
        <font size="-2">
          Copyright &copy; $copyright. All Rights Reserved.<br>
          Generated by <a href="$scandocURL"><b>ScanDoc
          $majorVersion.$minorVersion</b></a> on $date
        </font>
      </body>
    </html>
    <<
}
