<?xml version="1.0"?>

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/TR/xhtml1/strict">

  <!--
    Let's start by declaring HOW this stylesheet must behave.
  -->
  <xsl:output method="html" indent="no"
    doctype-public="-//W3C//DTD HTML 4.01//EN"
    doctype-system="http://www.w3.org/TR/html4/strict.dtd"/>

  <!--
    Match the ROOT of the source document and process its "document" element.
  -->
  <xsl:template match="/">
    <xsl:apply-templates select="document"/>
  </xsl:template>

  <!--
    Match the roote "document" element, let's prepare the layout of the whole
    page.
  -->
  <xsl:template match="document">
    <html>

      <!--
        This is the page header, we want a title from this document title
        the <meta> copyright statement and all authors in "meta" headers.
      -->
      <head>
        <title>
          <xsl:if test="string-length(description/text()) = 0">
            <xsl:value-of select="@title"/>
          </xsl:if>
          <xsl:value-of select="description/text()"/>
        </title>
        <meta name="copyright" content="1999-2002 The Apache Software Foundation"/>
        <xsl:for-each select="author">
          <meta name="author" content="text()"/>
          <meta name="author" content="@email"/>
        </xsl:for-each>
        <link rel="stylesheet" type="text/css" href="style.css"/>
      </head>

      <!--
        This describes the layout of the page
      -->
      <body bgcolor="#ffffff" text="#000000" alink="#666666" vlink="#333333" link="#666666">
        <a name="TOP"/>

        <table border="0" cellspacing="0" cellpadding="0" width="100%">
          <!--
            An empty row (thank you stupid IE).
          -->
          <tr height="1">
            <td width="150" bgcolor="#666666" height="1" class="nil">
              <img src="images/pixel.gif" border="0" width="150" height="1" vspace="0" hspace="0"/>
            </td>
            <td width="*" bgcolor="#666666" height="1" class="nil">
              <img src="images/pixel.gif" border="0" width="570" height="1" vspace="0" hspace="0"/>
            </td>
          </tr>

          <!--
            Our first row contains the Jakarta and the WebApp logos.
          -->
          <tr>
            <td bgcolor="#666666" class="logo" colspan="2" width="*">
              <table border="0" cellspacing="0" cellpadding="0" width="100%">
                <tr>
                  <td align="left">
                    <img src="images/jakarta.gif" border="0" width="270" height="75" align="left"/>
                  </td>
                  <td align="right">
                    <img src="images/webapp.gif" border="0" width="400" height="75" align="right"/>
                  </td>
                </tr>
              </table>
            </td>
          </tr>

          <!--
            A Turbine-style bar with links to the ASF, Jakarta and Tomcat.
          -->
          <tr>
            <td bgcolor="#999999" class="head" align="right" width="*" colspan="2">
              <nobr>
                <a class="head" href="http://www.apache.org/">
                  <xsl:text>Apache Software Foundation</xsl:text>
                </a> |
                <a class="head" href="http://jakarta.apache.org/">
                  <xsl:text>Jakarta Project</xsl:text>
                </a> |
                <a class="head" href="http://jakarta.apache.org/tomcat/">
                  <xsl:text>Apache Tomcat</xsl:text>
                </a>
              </nobr>
            </td>
          </tr>

          <!--
            Sidebar menu in a nested table and main content.
          -->
          <tr>
            <td bgcolor="#ffffff" width="150" valign="top">

              <!--
                This is the sidebar menu, we have links to all documents specified
                in "menu.idx", and if this is the current document, we go deeper
                and write an index of the sections as well.
              -->
              <table border="0" width="150" cellspacing="0" cellpadding="0" class="menu">
                <!-- Empty row, thanks IE -->
                <tr height="1">
                  <td width="10" bgcolor="#cccccc" height="1" class="nil">
                    <img src="images/pixel.gif" border="0" width="10" height="1" vspace="0" hspace="0"/>
                  </td>
                  <td width="140" bgcolor="#cccccc" height="1" class="nil">
                    <img src="images/pixel.gif" border="0" width="140" height="1" vspace="0" hspace="0"/>
                  </td>
                </tr>

                <!--
                  All the files we want to have processed in the final pages are
                  stored (in order) in a file called "menu.idx". We set a variable
                  name with the current URL, and then we process each "document"
                  within the index.
                -->
                <xsl:variable name="root" select="document-location(.)"/>
                <xsl:for-each select="document('menu.idx')/index/document">
                  <tr>
                    <td bgcolor="#cccccc" width="150" colspan="2">
                      <nobr>
                        <a class="menu">
                          <xsl:call-template name="converturi">
                            <xsl:with-param name="href" select="@href"/>
                          </xsl:call-template>
                        </a>
                      </nobr>
                    </td>
                  </tr>

                  <!--
                    Slightly more complicated, we use the document-location function
                    and compare against it to see whether we are in the same file or
                    not. If we actually are, we expand to the "section" level.
                  -->
                  <xsl:if test="$root = document-location(document(@href))">
                    <xsl:for-each select="document(@href)/document/section">
                      <tr>
                        <td bgcolor="#cccccc" width="10"/>
                        <td bgcolor="#cccccc" width="140">
                          <a class="menu" href="#section_{position()}">
                            <xsl:value-of select="@title"/>
                          </a>
                        </td>
                      </tr>
                    </xsl:for-each>
                  </xsl:if>
                </xsl:for-each>

                <!--
                  The last thing to put down in the index are the API docs,
                  both for C and for Java
                -->
                <tr>
                  <td bgcolor="#cccccc" width="150" colspan="2">
                    <nobr>
                      <a class="menu" href="./api-java/index.html">Java API Documentation</a>
                    </nobr>
                  </td>
                </tr>
                <tr>
                  <td bgcolor="#cccccc" width="150" colspan="2">
                    <nobr>
                      <a class="menu" href="./api-c/">C API Documentation</a>
                    </nobr>
                  </td>
                </tr>
              </table>
            </td>

            <!--
              Done with the sidebar, now, do we want some content as well or WHAT?
            -->
            <td bgcolor="#ffffff" width="*" valign="top" class="body">
              <xsl:apply-templates select="section"/>
            </td>
          </tr>
        </table>
      </body>
    </html>
  </xsl:template>

  <!--
    Match the "author" tag only in mode "header" (meaning that we have to
    process it for the HTML <head> element.
  -->
  <xsl:template match="author" mode="header">
    <meta name="author" content="{text()}"/>
    <meta name="email" content="{@email}"/>
  </xsl:template>

  <!--
    Present a canonical representation of an author.
  -->
  <xsl:template match="author">
    <a href="mailto:{@email}"><xsl:value-of select="text()"/></a>
  </xsl:template>

  <xsl:template match="section">
    <a name="section_{position()}">
      <table border="0" cellspacing="0" cellpadding="0" width="100%">
        <tr>
          <td bgcolor="#666666" class="section" valign="top" align="left">
            <img src="images/corner.gif" valign="top" align="left" hspace="0" vspace="0" border="0"/>
              <xsl:if test="string-length(description/text()) = 0">
                <xsl:value-of select="@title"/>
              </xsl:if>
              <xsl:value-of select="description/text()"/>
          </td>
        </tr>
      </table>
    </a>
    <xsl:apply-templates select="p|screen|todo"/>
    <br/>
  </xsl:template>

  <xsl:template match="todo">
    <p class="todo">
      This paragraph has not been written yet, but <b>you</b> can contribute to it.
      <xsl:if test="string-length(@note) > 0">
        The original author left a note attached to this TO-DO item:
        <b><xsl:value-of select="@note"/></b>
      </xsl:if>
    </p>
  </xsl:template>

  <xsl:template match="p">
    <p class="section"><xsl:apply-templates select="b|text()"/></p>
  </xsl:template>

  <xsl:template match="b">
    <b><font color="#333333"><xsl:apply-templates select="text()"/></font></b>
  </xsl:template>

  <xsl:template match="br">
    <br/>
  </xsl:template>

  <xsl:template match="screen">
    <p class="screen">
      <div align="center">
        <table width="80%" border="1" cellspacing="0" cellpadding="2" bgcolor="#cccccc">
          <tr>
            <td bgcolor="#cccccc">
              <xsl:apply-templates select="note|wait|type|read"/>
            </td>
          </tr>
        </table>
      </div>
    </p>
  </xsl:template>
  
  <xsl:template match="note">
    <div class="screen">
      <xsl:value-of select="text()"/>
    </div>
  </xsl:template>

  <xsl:template match="wait">
    <div class="screen">[...]</div>
  </xsl:template>

  <xsl:template match="type">
    <code>
      <nobr>
        <em class="screen">
          <xsl:text>[user@host] ~</xsl:text>
          <xsl:if test="string-length(@dir) > 0">
            <xsl:text>/</xsl:text>
            <xsl:value-of select="@dir"/>
          </xsl:if>
          <xsl:text> $ </xsl:text>
        </em>
        <xsl:if test="string-length(text()) > 0">
          <b class="screen"><xsl:value-of select="text()"/></b>
        </xsl:if>
      </nobr>
    </code>
    <br/>
  </xsl:template>

  <xsl:template match="read">
    <code>
      <nobr>
        <xsl:apply-templates select="text()|enter"/>
      </nobr>
    </code>
    <br/>
  </xsl:template>

  <xsl:template match="enter">
    <b class="screen"><xsl:value-of select="text()"/></b>
  </xsl:template>

  <xsl:template match="a">
    <b>
      <a>
        <xsl:call-template name="converturi">
          <xsl:with-param name="href" select="@href"/>
          <xsl:with-param name="text" select="text()"/>
          <xsl:with-param name="attr" select="'href'"/>
        </xsl:call-template>
      </a>
    </b>
  </xsl:template>

  <!--
    Convert the name of the matching "href" attribute (if needed) from
    "file.xml#anchor" to "file.html#anchor", and insert the title of
    the target document as the only text child of the resulting html
    <a /> tag. (Of course, don't convert fully qualified URIs).
  -->
  <xsl:template name="converturi">
    <xsl:param name="attr" select="'href'"/>
    <xsl:param name="href" select="''"/>
    <xsl:param name="text" select="''"/>

    <xsl:choose>
    
      <!--
        If the "href" parameter contains ":" this is most definitely an URL,
        therefore we need to quote it "as is" without translating its value.
        The text is either supplied, or it's the value of the URL itself
        (without the trailing anchor, if any).
      -->
      <xsl:when test="contains($href,':')">
        <xsl:attribute name="{$attr}"><xsl:value-of select="$href"/></xsl:attribute>
        <xsl:if test="string-length($text) = 0">
          <xsl:choose>
            <xsl:when test="contains($href,'#')">
              <xsl:value-of select="substring-before($href,'#')"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="$href"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:if>
        <xsl:value-of select="$text"/>
      </xsl:when>

      <!--
        Nope, we don't have a full URL, therefore we interpret this as a
        relative hyperlink to another document. We need to translate its
        name from "*.xml" to "*.html" (because this is how we convert the
        names) and the text included in this will be the title of the
        target document.
      -->
      <xsl:otherwise>
        <!--
          The "file" variable contains the part of the "href" before
          the "#" character. Yes, the "file" name.
        -->
        <xsl:variable name="file">
          <xsl:choose>
            <xsl:when test="contains($href,'#')">
              <xsl:value-of select="substring-before($href,'#')" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="$href" />
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>

        <!--
          Like "file" the "anchor" variable contains the part of the "href"
          after the "#" character.
        -->
        <xsl:variable name="anchor">
          <xsl:if test="contains($href,'#')">
            <xsl:value-of select="'#'" />
            <xsl:value-of select="substring-after($href,'#')" />
          </xsl:if>
        </xsl:variable>

        <!--
          Good, now we check if "file" ends in ".xml", if so, we replace that
          with ".html", otherwise we keep its original value, then we add the
          anchor we gathered before. We call this "target".
        -->
        <xsl:variable name="target">
          <xsl:if test="string-length($file) > 0">
            <xsl:choose>
              <xsl:when test="substring($file,string-length($file)-3) = '.xml'">
                <xsl:value-of select="substring($file,1,string-length($file)-3)"/>
                <xsl:value-of select="'html'"/>
              </xsl:when>
              <xsl:otherwise>
                <xsl:value-of select="$file"/>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:if>
          <xsl:value-of select="$anchor"/>
        </xsl:variable>

        
        <!--
          Now, we want to set the attribute to contain the "target" variable.
        -->
        <xsl:attribute name="{$attr}">
          <xsl:value-of select="$target"/>
        </xsl:attribute>

        <!--
          To finish we want to set the body of this element: if we have "text"
          the body of the element will be just that, otherwise, it will be
          the "target" value (the translated href) if there was no text,
          or the "title" of the target document if we actually translated
          something
        -->
        <xsl:if test="string-length($text) = 0">
          <xsl:choose>
            <xsl:when test="$target = $href">
              <xsl:value-of select="$file"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="document($file)/document/@title"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:if>
        <xsl:value-of select="$text"/>

      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
