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

if ($project eq "") { $project = "WebApp Library"; }
if ($copyright eq "") { $copyright = "2001, The Apache Software Foundation"; }
if ($body_bgcolor eq "") { $body_bgcolor = '#ffffff'; }
if ($body_text    eq "") { $body_text    = '#000000'; }
if ($body_link    eq "") { $body_link    = '#0000ff'; }
if ($body_vlink   eq "") { $body_vlink   = '#0000ff'; }
if ($body_alink   eq "") { $body_alink   = '#0000ff'; }

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
    <<

    # Generate a list of classes included in this package
    if ($p->classes()) {
        >>
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
    } else {
        >>
              <tr>
                <td colspan="2">
                  <font face="arial,helvetica,sans serif">
                    <nobr><i>No Classes defined.</i></nobr>
                  </font>
                </td>
              </tr>
        <<
    }

    # Generate a list of all global functions included in this package
    >>
      <tr><td colspan="2">&nbsp;</td></tr>
    <<
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

    # Generate a list of all global variables included in this package
    >>
      <tr><td colspan="2">&nbsp;</td></tr>
    <<
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

    # Copyright statement at the bottom
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
    <<

    # Generate a TOC of all classes at the top of the page
    if ($p-> classes()) {
        >>
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
                            <nobr><a href="doc.$_">$(e.fullname)</a></nobr>
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
                                    <nobr><a href="doc.$_">$(m.fullname)</a></nobr>
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
    }

    # Continue with the global functions TOC
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
                        <nobr><a href="doc.$_">$(e.fullname)</a></nobr>
                      </font>
                    </td>
                  </tr>
            <<
        }
    }

    # And then finish with the global variables TOC
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
                        <nobr><a href="doc.$_">$(e.fullname)</a></nobr>
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

    # Then generate the detail for each class in this package
    foreach $e ($p->classes()) {
        $_ = $e->url;
        s/\s/_/g;
        y/[A-Z]/[a-z]/;
        >>
            <table width="100%" cellspacing="0" cellpadding="2" border="1">
              <tr>
                <td bgcolor="ccccff" align="left">
                  <font size="+1" face="arial,helvetica,sans serif">
                    <a name="$(e.name)">
                      <b>Class &quot;$(e.name)&quot; Detail:</b>
                    </a>
                  </font>
                </td>
              </tr>
            </table>
              <font size="+1" face="arial,helvetica,sans serif">
                <b>$(e.name)</b>
              </font>
            <dl>
              <dt><code>$(e.fullname) {</code></dt>
        <<

        # Generate a code-like representation of the class
        if ($e->members()) {
            foreach $m ($e->members()) {
                >>
                      <dd><code>$(m.fullname);</code></dd>
                <<
            }
        }
        >>
              <dt><code>};</code></dt>
            </dl>
            <p>
              <font face="arial,helvetica,sans serif">
                $(e.description)
              </font>
            </p>
        <<

        # If we have functions, output them
        if ($e->memberfuncs()) {
            >>
                <table width="100%" cellspacing="0" cellpadding="2" border="1">
                  <tr>
                    <td bgcolor="eeeeff" align="left">
                      <font face="arial,helvetica,sans serif">
                        <b>Class &quot;$(e.name)&quot; Functions:</b>
                      </font>
                    </td>
                  </tr>
                </table>
            <<
            foreach $m ($e->memberfuncs()) {
                $_ = join("-",$e->name,$m->name);
                s/\s/_/g;
                y/[A-Z]/[a-z]/;
                >>
                    <a name="$_">
                <<
                &function($m);
            }
        }

        # If we have variables, output them
        if ($e->membervars()) {
            >>
                <table width="100%" cellspacing="0" cellpadding="2" border="1">
                  <tr>
                    <td bgcolor="eeeeff" align="left">
                      <font face="arial,helvetica,sans serif">
                        <b>Class &quot;$(e.name)&quot; Variables:</b>
                      </font>
                    </td>
                  </tr>
                </table>
            <<
            foreach $m ($e->membervars()) {
                $_ = join("-",$e->name,$m->name);
                s/\s/_/g;
                y/[A-Z]/[a-z]/;
                >>
                    <a name="$_">
                <<
                &variable($m);
            }
        }
    }

    # Output detailed information for each global function
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
            $_ = $e->name;
            s/\s/_/g;
            y/[A-Z]/[a-z]/;
            >>
                <a name="$_">
            <<
            &function($e);
        }
    }

    # Then write detailed information for each global variable
    if ($p->globalvars()) {
        >>
            <table width="100%" cellspacing="0" cellpadding="2" border="1">
              <tr>
                <td bgcolor="ccccff" align="left">
                  <font size="+1" face="arial,helvetica,sans serif">
                    <b>Global Variables Detail:</b>
                  </font>
                </td>
              </tr>
            </table>
        <<
        foreach $e ($p->globalvars()) {
            $_ = $e->name;
            s/\s/_/g;
            y/[A-Z]/[a-z]/;
            >>
                <a name="$_">
            <<
            &variable($e);
        }
    }

    >>
        <font size="-2" face="arial,helvetica,sans serif">
          <div align="center">
            Copyright &copy; $copyright.<br>
            All Rights Reserved.<br>
            Generated with <a href="$scandocURL">ScanDoc
            $majorVersion.$minorVersion</a> on $date
          </div>
        </font>
      </body>
    </html>
    <<
}

# Write out the detailed description of a function
sub function {
    local ($m) = @_;

    # Output the function name and description
    >>
        <font size="+1" face="arial,helvetica,sans serif">
          <b>$(m.name)</b>
        </font>
        <dl>
          <dt><code>$(m.fullname);</code></dt>
          <dd>
            <dl>
              <dt>
                <font face="arial,helvetica,sans serif">
                  $(m.description)
                </font>
              </dt>
    <<

    # Process all parameters (one by one)
    if ($m->params()) {
        >>
                  <dt>
                    <font face="arial,helvetica,sans serif">
                      <b>Parameters</b>
                    </font>
                  </dt>
        <<
        foreach $a ($m->params()) {
            >>
                      <dd>
                        <code>$(a.name)</code> -
                        <font face="arial,helvetica,sans serif">
                          $(a.description)
                        </font>
                      </dd>
            <<
        }
    }

    # Check for a return value
    if ($m->returnValue()) {
        >>
                  <dt>
                    <font face="arial,helvetica,sans serif">
                      <b>Return Value</b>
                    </font>
                  </dt>
                  <dd>
                    <font face="arial,helvetica,sans serif">
                      $(m.returnValue)
                    </font>
                  </dd>
        <<
    }

    # Dig in for exceptions
    if ($m->exceptions()) {
        >>
                  <dt>
                    <font face="arial,helvetica,sans serif">
                      <b>Exceptions</b>
                    </font>
                  </dt>
        <<
        foreach $a ($m->exceptions()) {
            >>
                      <dd>
                        <code>$(a.name)</code>
                        <font face="arial,helvetica,sans serif">
                          $(a.description)
                        </font>
                      </dd>
            <<
        }
    }

    # Close the list
    >>
            </dl>
          </dd>
        </dl>
        <hr>
    <<
}

# Write out the detailed description of a variable
sub variable {
    local ($m) = @_;

    # Output the variable name and description
    >>
        <font size="+1" face="arial,helvetica,sans serif">
          <b>$(m.name)</b>
        </font>
        <dl>
          <dt><code>$(m.fullname);</code></dt>
          <dd>
            <dl>
              <dt>
                <font face="arial,helvetica,sans serif">
                  $(m.description)
                </font>
              </dt>
            </dl>
          </dd>
        </dl>
        <hr>
    <<
}
