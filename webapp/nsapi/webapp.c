#include <base/pblock.h>
#include <base/util.h>

#include <wa.h>

void wa_log(const char *f, const int l, const char *fmt, ...) { }

/* exported functions */
int init_webapp(pblock * pb, Session * sn, Request * rq);
int nametrans_webapp(pblock * pb, Session * sn, Request * rq);
int service_webapp(pblock * pb, Session * sn, Request * rq);
int snoop(pblock * pb, Session * sn, Request * rq);

/* private functions */
void processConfigFile(pblock * pb, Session * sn, Request * rq, char * filename);
void processLine(pblock * pb, Session * sn, Request * rq, char * line);
void processDirective(pblock * pb, Session * sn, Request * rq, char * directive, char * arguments);
void processWebAppInfoDirective(pblock * pb, Session * sn, Request * rq, char * arguments);
void processWebAppConnectionDirective(pblock * pb, Session * sn, Request * rq, char * arguments);
void processWebAppDeployDirective(pblock * pb, Session * sn, Request * rq, char * arguments);
static const char * webAppInfo(pblock * pb, Session * sn, Request * rq, char * path);
static const char * webAppConnection(pblock * pb, Session * sn, Request * rq, char * name, char * prov, char * p);
static const char * webAppDeploy(pblock * pb, Session * sn, Request * rq, char * name, char * cnam, char * path);
static int readPostData(pblock * pb, Session * sn, Request * rq, char * io_buf, int readsz, int * bytes);

/* Whether the WebApp Library has been initialized or not */
static wa_boolean wam_initialized = wa_false;

/* The list of configured connections */
static wa_chain *wam_connections = NULL;

static wa_virtualhost * hackhost = NULL;

typedef struct iplanet_stuff {
    pblock * pb;
    Session * sn;
    Request * rq;
} iplanet_stuff;

/* Log a message associated with a request */
void wam_handler_log(wa_request * r, const char * f, const int l, char * msg) {
    char * fn = (char *) f;
    Session * sn = ((iplanet_stuff *) r->data)->sn;
    Request * rq = ((iplanet_stuff *) r->data)->rq;

    log_error(LOG_INFORM, fn, sn, rq, msg);
}

/* Set the HTTP status of the response. */
void wam_handler_setstatus(wa_request * r, int status) {
    Session * sn = ((iplanet_stuff *) r->data)->sn;
    Request * rq = ((iplanet_stuff *) r->data)->rq;

    protocol_status(sn, rq, status, NULL);
}

/* Set the MIME Content-Type of the response. */
void wam_handler_setctype(wa_request * r, char * type) {
    Request * rq = ((iplanet_stuff *) r->data)->rq;

    if (pblock_findval("content-type", rq->srvhdrs))
	    param_free(pblock_remove("content-type", rq->srvhdrs));

    pblock_nvinsert("content-type", type, rq->srvhdrs);
}

/* Set a header in the HTTP response. */
void wam_handler_setheader(wa_request * r, char * name, char * value) {
    Request * rq = ((iplanet_stuff *) r->data)->rq;

    pblock_nvinsert(name, value, rq->srvhdrs);
}

/* Commit the first part of the response (status and headers). */
void wam_handler_commit(wa_request * r) {
    Session * sn = ((iplanet_stuff *) r->data)->sn;
    Request * rq = ((iplanet_stuff *) r->data)->rq;

    /* int responseStatus = */ protocol_start_response(sn, rq);

/* don't know how to fix this...
 *     if (responseStatus == REQ_ABORTED)
 *     	return REQ_ABORTED;
 *     else if (responseStatus == REQ_NOACTION)
 * 	return REQ_PROCEED;
 */
}

/* Flush all data in the response buffer. */
void wam_handler_flush(wa_request * r) {
    /* what do I do here? */
}

/* Read a chunk of text from the request body. */
int wam_handler_read(wa_request * r, char * buf, int len) {
    Session * sn = ((iplanet_stuff *) r->data)->sn;

    int ch;
    int inbytes;

    for (inbytes = 0; inbytes < len; inbytes++) {
        ch = netbuf_getc(sn->inbuf);
        if (ch == IO_ERROR)
            return 0; /* failed */

        buf[inbytes] = (char) ch;
    }

    return 0; /* succeeded */
}

/* Write a chunk of text into the response body. */
int wam_handler_write(wa_request * r, char * buf, int len) {
    Session * sn = ((iplanet_stuff *) r->data)->sn;

    /* int status = */ net_write(sn->csd, buf, len);
/*
 *     if (status == IO_ERROR)
 *         return REQ_EXIT;
 */
    return 0;
}

/* The structure holding all callback handling functions for the library. */
static wa_handler wam_handler = {
    wam_handler_log,
    wam_handler_setstatus,
    wam_handler_setctype,
    wam_handler_setheader,
    wam_handler_commit,
    wam_handler_flush,
    wam_handler_read,
    wam_handler_write,
};

/* Initialize the module and the WebApp Library. */
static const char * wam_init(void) {
    const char * ret = NULL;

    if (wam_initialized == wa_true)
    	return NULL;

    if ((ret = wa_init()))
    	return ret;

    if ((ret = wa_cvirtualhost(&hackhost, "hack-host", 80)))
    	return ret;
	
    wam_initialized = wa_true;
    
    return NULL;
}

static void wam_startup() {
    if (!wam_initialized)
    	return;

    wa_startup();
}

int verbose = 0;

int init_webapp(pblock * pb, Session * sn, Request * rq) {
    char * fn = "init-webapp";

    char * config = pblock_findval("config", pb);
    char * verboseParam = pblock_findval("verbose", pb);

    if (!config) {
	pblock_nvinsert("error", "missing parameter (config)", pb);
	return REQ_ABORTED;
    }

    verbose = verboseParam && !strcmp(verboseParam, "true");

    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq, "in init-webapp");
	log_error(LOG_INFORM, fn, sn, rq, "config = '%s'", config);
    }

    processConfigFile(pb, sn, rq, config);

/*
    if (!processConfigFile(config)) {
	pblock_nvinsert("error", "error reading config", pb);
	return REQ_ABORTED;
    }
*/

    wam_init( /* ... */ );

    wam_startup();

    return REQ_PROCEED;
}

#define MAX_LINE_LENGTH 8096

void processConfigFile(pblock * pb, Session * sn, Request * rq,
    	    	       char * filename)
{
    char * fn = "webapp-init <processConfigFile>";

    FILE * fp;
    char foo[MAX_LINE_LENGTH];
    char c;

    if (verbose)
    	log_error(LOG_INFORM, fn, sn, rq, "in <processConfigFile>");

    /* open the config file */
    if (!(fp = fopen(filename, "r"))) {
    	log_error(LOG_MISCONFIG, fn, sn, rq, "fopen failed on '%s'", filename);
	return;
    }

    while (!feof(fp)) {
	if ((c = fgetc(fp)) == EOF)
	    break;

	if (c == '#') {
	    /* skip over comment lines */
	    fgets(foo, MAX_LINE_LENGTH, fp);
	} else if (c == '\r' || c == '\n') {
	    /* eat up any newlines or carriage returns */
	    continue;
	} else {
	    foo[0] = c;
	    fgets(&foo[1], MAX_LINE_LENGTH - 1, fp);
    	    processLine(pb, sn, rq, foo);
	}
    }

    fclose(fp);
}

void processLine(pblock * pb, Session * sn, Request * rq, char * line) {
    char * fn = "webapp-init <processLine>";

    char * foo;
    char * space;
    
    char * directive;
    char * arguments;

    if (verbose)
    	log_error(LOG_INFORM, fn, sn, rq, "in <processLine>, line = '%s'", line);

    /* eat up any trailing carriage returns or newlines */
    for (foo = line + strlen(line) - 1;
	     *foo == '\r' || *foo == '\n';
	     --foo)
    {
	*foo = '\0';
    }

    directive = line;

    if ((space = strchr(line, ' '))) {
	*space = '\0';
	arguments = space + 1;
    } else
	arguments = line + strlen(line); /* i.e. empty */

    processDirective(pb, sn, rq, directive, arguments);
}

void processDirective(pblock * pb, Session * sn, Request * rq,
    	    	      char * directive, char * arguments)
{
    char * fn = "webapp-init <processConfigFile>";

    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq,
	    	  "in <processDirective>, directive = '%s', arguments = '%s'",
		  directive, arguments ? arguments : "(null)");
    }
    
    if (!strcmp(directive, "WebAppInfo"))
    	processWebAppInfoDirective(pb, sn, rq, arguments);
    else if (!strcmp(directive, "WebAppConnection"))
    	processWebAppConnectionDirective(pb, sn, rq, arguments);
    else if (!strcmp(directive, "WebAppDeploy"))
    	processWebAppDeployDirective(pb, sn, rq, arguments);
    else {
    	log_error(LOG_MISCONFIG, fn, sn, rq,
	    	  "Unrecognised directive: '%s'", directive);
    }
}

void processWebAppInfoDirective(pblock * pb, Session * sn, Request * rq,
    	    	    	    	char * arguments) {
    char * fn = "webapp-init <processWebAppInfoDirective>";

    char * uriPath;

    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq,
	    	  "in <processWebAppInfoDirective>, arguments = '%s'",
		  arguments ? arguments : "(null)");
    }
    
    if (!arguments || !strlen(arguments))
    	return;

    uriPath = arguments;

    webAppInfo(pb, sn, rq, uriPath);
}

void processWebAppConnectionDirective(pblock * pb, Session * sn, Request * rq,
    	    	    	    	      char * arguments)
{
    char * fn = "webapp-init <processWebAppConnectionDirective>";

    char * name;
    char * provider;
    char * optionalParameter;
    char * space;
    
    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq,
	    	  "in <processWebAppConnectionDirective>, arguments = '%s'",
		  arguments ? arguments : "(null)");
    }

    if (!arguments || !strlen(arguments))
    	return;

    name = arguments;
    
    if (!(space = strchr(name, ' ')))
    	return;
    
    *space = '\0';
    provider = space + 1;
    
    if ((space = strchr(provider, ' '))) {
    	*space = '\0';
    	optionalParameter = space + 1;
    } else
    	optionalParameter = NULL;
    
    webAppConnection(pb, sn, rq, name, provider, optionalParameter);
}

void processWebAppDeployDirective(pblock * pb, Session * sn, Request * rq,
    	    	    	    	  char * arguments)
{
    char * fn = "webapp-init <processWebAppDeployDirective>";

    char * name;
    char * connection;
    char * uriPath;
    char * space;
    
    const char * ret;
    
    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq,
	    	  "in <processWebAppDeployDirective>, arguments = '%s'",
		  arguments ? arguments : "(null)");
    }

    if (!arguments || !strlen(arguments))
    	return;

    name = arguments;
    
    if (!(space = strchr(name, ' ')))
    	return;
    
    *space = '\0';
    connection = space + 1;
    
    if (!(space = strchr(connection, ' ')))
    	return;
    
    *space = '\0';
    uriPath = space + 1;
    
    if ((ret = webAppDeploy(pb, sn, rq, name, connection, uriPath)))
    	log_error(LOG_MISCONFIG, fn, sn, rq, "webAppDeploy reports: '%s'", ret);
}

/* Process the WebAppInfo directive */
static const char * webAppInfo(pblock * pb, Session * sn, Request * rq,
    	    	    	       char * path)
{
    char * fn = "webapp-init <webAppInfo>";

    const char * ret;

    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq,
	    	  "in <webAppInfo>, path = '%s'", path ? path : "(null)");
    }

    /* divert this call to a WebAppConnection and a WebAppDeploy call */
    if ((ret = webAppConnection(pb, sn, rq, "_INFO_", "info", "")))
        return ret;
	
    if ((ret = webAppDeploy(pb, sn, rq, "_INFO_", "_INFO_", path)))
        return ret ;

    return NULL;
}

/* Process the WebAppConnection directive. */
static const char * webAppConnection(pblock * pb, Session * sn, Request * rq,
    	    	    	    	     char * name, char * prov, char * p)
{
    char * fn = "webapp-init <webAppConnection>";

    wa_connection * conn = NULL;
    const char * ret = NULL;
    wa_chain * elem = NULL;

    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq,
	    	  "in <webAppConnection>, name = '%s', prov = '%s', p = '%s'",
		  name ? name : "(null)", prov ? prov : "null", p ? p : "null");
    }

    /* Initialize the library */
    if ((ret = wam_init( /* cmd->pool */ )))
    	return ret;

    /* Attempt to create a new wa_connection structure */
    if ((ret = wa_cconnection(&conn, name, prov, p)))
    	return ret;

    /* Check if we have a duplicate connection with this name */
    elem = wam_connections;
    while (elem) {
        wa_connection * curr = (wa_connection *) elem->curr;
        if (!strcasecmp(conn->name, curr->name))
            return "Duplicate connection name";
        elem = elem->next;
    }

    /* We don't have a duplicate connection, store it locally */
    elem = apr_palloc(wa_pool, sizeof(wa_chain));
    elem->curr = conn;
    elem->next = wam_connections;
    wam_connections = elem;
    
    return NULL;
}

/* Process the WebAppDeploy directive */
static const char * webAppDeploy(pblock * pb, Session * sn, Request * rq,
    	    	    	    	 char * name, char * cnam, char * path)
{
    char * fn = "webapp-init <webAppDeploy>";

    wa_virtualhost * host = NULL;
    wa_application * appl = NULL;
    wa_connection * conn = NULL;
    wa_chain * elem = NULL;
    const char * ret = NULL;

    if (verbose) {
    	log_error(LOG_INFORM, fn, sn, rq,
	    	  "in <webAppConnection>, "
		  "name = '%s', cnam = '%s', path = '%s'",
		  name ? name : "(null)",
		  cnam ? cnam : "null",
		  path ? path : "null");
    }

    /* Initialize the library and retrieve/create the host structure */
    if ((ret = wam_init( /* cmd->pool */ )) /* ||
    	(ret = wam_server(cmd->server, &host)) */ )
    {
	return ret;
    }

    host = hackhost; /* HACK HACK HACK */

    /* Retrieve the connection */
    elem = wam_connections;
    while (elem) {
        wa_connection * curr = (wa_connection *) elem->curr;
        if (!strcasecmp(curr->name, cnam)) {
            conn = curr;
            break;
        }
        elem = elem->next;
    }
    if (!conn)
    	return "Specified connection not configured";

    /* Create a new wa_application member */
    if ((ret = wa_capplication(&appl, name, path)))
    	return ret;

    appl->host = hackhost; /* HACK! HACK! HACK! */

    /* Deploy the web application */
    if ((ret = wa_deploy(appl, host, conn)))
    	return ret;

    /* Done */
    return NULL;
}

int nametrans_webapp(pblock * pb, Session * sn, Request * rq) {
    char * fn = "nametrans-webapp";

    wa_virtualhost * host = NULL;
    wa_application * appl = NULL;
    wa_chain * elem = NULL;

    char * uri = pblock_findval("uri", rq->reqpb);

    if (verbose)
    	log_error(LOG_INFORM, fn, sn, rq, "in nametrans-webapp");

    /* Paranoid check */
    if (!wam_initialized) {
    	log_error(LOG_MISCONFIG, fn, sn, rq, "!wam_initialized");
	return REQ_ABORTED;
    }

    /* Check if this host was recognized */
/*
 *  if (!(host = ap_get_module_config(r->server->module_config,
 *  	    	    	    	      &webapp_module)))
 *  {
 *     	return REQ_NOACTION;
 *  }
 */
    host = hackhost; /* HACK HACK HACK */

    /* Check if the uri is contained in one of our applications root path */
    elem = host->apps;
    while (elem) {
        appl = (wa_application *) elem->curr;
	
	if (verbose) {
	    log_error(LOG_INFORM, fn, sn, rq,
	    	      "comparing mapping '%s' to uri '%s'",
		      appl->rpth, uri);
	}
	
        if (!strncmp(appl->rpth, uri, strlen(appl->rpth)))
	    break;

        appl = NULL;
        elem = elem->next;
    }

    if (!appl)
    	return(REQ_NOACTION);

    if (verbose)
	log_error(LOG_INFORM, fn, sn, rq, "changing name to webapp");

    if (pblock_findval("name", rq->vars))
	param_free(pblock_remove("name", rq->vars));

    pblock_nvinsert("name", "webapp", rq->vars);

    return REQ_PROCEED;
}

int service_webapp(pblock * pb, Session * sn, Request * rq) {
    char * fn = "service-webapp";

    wa_virtualhost * host = NULL;
    wa_application * appl = NULL;
    wa_chain * elem = NULL;

    char * uri = pblock_findval("uri", rq->reqpb);
    wa_request * req = NULL;
    iplanet_stuff * stuff;
    const char * msg = NULL;
    char *stmp = NULL;
    char *ctmp = NULL;
    int ret = 0;
    char * np, * vp, * name, * value, * headers;

    if (verbose)
    	log_error(LOG_INFORM, fn, sn, rq, "in service-webapp");

    /* Paranoid check */
    if (!wam_initialized) {
    	log_error(LOG_MISCONFIG, fn, sn, rq, "!wam_initialized");
	return REQ_ABORTED;
    }

    /* Check if this host was recognized */
/*
 *  if (!(host = ap_get_module_config(r->server->module_config,
 *  	    	    	    	      &webapp_module)))
 *  {
 *     	return REQ_NOACTION;
 *  }
 */
    host = hackhost; /* HACK HACK HACK */

    /* Check if the uri is contained in one of our applications root path */
    elem = host->apps;
    while (elem) {
        appl = (wa_application *) elem->curr;
	
	if (verbose) {
	    log_error(LOG_INFORM, fn, sn, rq,
	    	      "comparing mapping '%s' to uri '%s'",
		      appl->rpth, uri);
	}
	
        if (!strncmp(appl->rpth, uri, strlen(appl->rpth)))
	    break;

        appl = NULL;
        elem = elem->next;
    }

    if (!appl) {
    	log_error(LOG_MISCONFIG, fn, sn, rq, "!appl");
    	return REQ_NOACTION;
    }

    if (!(stuff = (iplanet_stuff *)
    	  apr_palloc(wa_pool, sizeof(iplanet_stuff))))
    {
	pblock_nvinsert("error", "Cannot allocate memory", pb);
	return REQ_ABORTED;
    }

    stuff->pb = pb;
    stuff->sn = sn;
    stuff->rq = rq;

    /* Allocate the webapp request structure */
    if ((msg = wa_ralloc(&req, &wam_handler, stuff))) {
	pblock_nvinsert("error", msg, pb);
	return REQ_ABORTED;
    }

    /* Set up the WebApp Library request structure client and server host
       data (from the connection) */
    stmp = "hack-host";
    if (!stmp)
    	req->serv->host = "";
    else
    	req->serv->host = apr_pstrdup(req->pool, stmp);

    ctmp = pblock_findval("ip", sn->client);
    if (!ctmp)
    	req->clnt->host = "";
    else
    	req->clnt->host = apr_pstrdup(req->pool, ctmp);

    req->serv->addr = "0.0.0.0";
    req->clnt->addr = pblock_findval("ip", sn->client);
    req->serv->port = conf_getglobals()->Vport;
    req->clnt->port = 88; /* how do I know? */
    /* Set up all other members of the request structure */
    req->meth = apr_pstrdup(req->pool,  pblock_findval("method", rq->reqpb));
    req->ruri = apr_pstrdup(req->pool, pblock_findval("uri", rq->reqpb));
    req->args = apr_pstrdup(req->pool, pblock_findval("query", rq->reqpb));
    req->prot = apr_pstrdup(req->pool, pblock_findval("protocol", rq->reqpb));
    req->schm = apr_pstrdup(req->pool, security_active ? "https" : "http");
    req->user = apr_pstrdup(req->pool, pblock_findval("auth-user", rq->vars));
    req->auth = apr_pstrdup(req->pool, pblock_findval("auth-type", rq->vars));
    req->clen = 0;
    req->ctyp = "\0";
    req->rlen = 0;

    /* Copy headers into webapp request structure */
    np = headers = pblock_pblock2str(rq->headers, NULL);
    while (*np) {
	if (!(vp = strchr(np, '='))) {
	    FREE(headers);
	    return REQ_ABORTED;
	}

	*vp = '\0';
	name = np;

	vp += 2;
	np = strchr(vp, '"');
	*np = '\0';
	value = vp;

	np++;
	if (*np == ' ') {
	    np++;
	}

        apr_table_add(req->hdrs, name, value);

        if (!strcasecmp(name, "Content-Length"))
            req->clen = atol(value);

        if (!strcasecmp(value, "Content-Type"))
            req->ctyp = apr_pstrdup(req->pool, value);
    }

    /* Check if we can read something from the request */
/*
 *   ret=ap_setup_client_block(r,REQUEST_CHUNKED_DECHUNK);
 *   if (ret!=OK) return(ret);
 */

    /* Invoke the request */
    ret = wa_rinvoke(req, appl);

    /* Destroy the request member */
    wa_rfree(req);
/*
 *  ap_rflush(r);
 */

    return(REQ_PROCEED);
}




int snoop(pblock * pb, Session * sn, Request * rq);

/* global constants */

const char * header =
    "<html>\n"
    "<head>\n"
    "<title>Snoop Plugin</title>\n"
    "</head>\n"
    "\n"
    "<body bgcolor=#FFFFFF>\n"
    "<font face=\"Arial,Helvetica\">\n"
    "\n"
    "<h2>Snoop Plugin</h2>\n"
    "\n"
    "<p>\n"
    "This NSAPI plugin returns information about the HTTP request.\n"
    "<p>\n";

const char * requestInformation =
    "<h3>\n"
    "Request information\n"
    "</h3>\n"
    "\n"
    "<b>Request Method:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Request URI:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Request Protocol:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Path Info:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Path Translated:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Query String:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Content Length:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Content Type:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Server Name:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Server Port:</b> <font face=\"Courier New, Courier, monospace\">%d</font><br>\n"
    "<b>Remote User:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Remote Address:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Remote Host:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n"
    "<b>Authorization Scheme:</b> <font face=\"Courier New, Courier, monospace\">%s</font>\n"
    "\n";

const char * requestHeaders =
    "<h3>\n"
    "Request headers\n"
    "</h3>\n"
    "\n";

const char * postData =
    "<h3>\n"
    "POST Data\n"
    "</h3>\n"
    "\n";

const char * footer =
    "</font>\n"
    "</body>\n"
    "</html>\n";

int snoop(pblock * pb, Session * sn, Request * rq) {
    char * fn = "snoop";

    char * verboseParam = pblock_findval("verbose", pb);
    int verbose = verboseParam && !strcmp(verboseParam, "true");

    int responseStatus;

    char buffer[8092];

    /* request information */
    
    char * requestMethod = pblock_findval("method", rq->reqpb);
    char * requestURI = pblock_findval("uri", rq->reqpb);
    char * protocol = pblock_findval("protocol", rq->reqpb);
    char * pathInfo = pblock_findval("path-info", rq->vars);
    char * pathTranslated = pblock_findval("path-translated", rq->vars);
    char * queryString = pblock_findval("query", rq->reqpb);
    char * contentLength = NULL;
    char * contentType = NULL;
    char * serverName = util_hostname();
    int serverPort = conf_getglobals()->Vport;
    char * authUser = pblock_findval("auth-user", rq->vars);
    char * remoteAddress = pblock_findval("ip", sn->client);
    char * remoteHost = session_dns(sn) ? session_dns(sn)
                                        : pblock_findval("ip", sn->client);
    char * authType = pblock_findval("auth-type", rq->vars);

    char * np, * vp, * name, * value, * headers;

    /* this is wasteful - I do it again when I print the headers out */

    np = headers = pblock_pblock2str(rq->headers, NULL);
    while (*np) {
	if (!(vp = strchr(np, '='))) {
	    FREE(headers);
	    return REQ_ABORTED;
	}

	*vp = '\0';
	name = np;

	vp += 2;
	np = strchr(vp, '"');
	*np = '\0';
	value = vp;

	np++;
	if (*np == ' ') {
	    np++;
	}

        if (!strcasecmp(name, "content-length"))
            contentLength = value;
        else if (!strcasecmp(name, "content-type"))
            contentType = value;
    }

    if (verbose) {
        log_error(LOG_INFORM, fn, sn, rq, "in snoop()");
    }

    if (pblock_findval("content-type", rq->srvhdrs))
        param_free(pblock_remove("content-type", rq->srvhdrs));
    
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);

    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    
    responseStatus = protocol_start_response(sn, rq);

    if (responseStatus == REQ_ABORTED)
        return REQ_ABORTED;
    else if (responseStatus == REQ_NOACTION)
        return REQ_PROCEED;

    if (net_write(sn->csd, (char *) header, strlen(header)) == IO_ERROR)
        return REQ_EXIT;

    util_sprintf(buffer, requestInformation,
                 requestMethod, requestURI, protocol,
                 pathInfo, pathTranslated, queryString, contentLength,
                 contentType, serverName, serverPort, authUser,
                 remoteAddress, remoteHost, authType);

    if (net_write(sn->csd, (char *) buffer, strlen(buffer)) == IO_ERROR)
        return REQ_EXIT;

    if (net_write(sn->csd,
                  (char *) requestHeaders,
                  strlen(requestHeaders)) == IO_ERROR)
    {
        return REQ_EXIT;
    }

    np = headers = pblock_pblock2str(rq->headers, NULL);
    while (*np) {
        if (!(vp = strchr(np, '='))) {
            FREE(headers);
            return REQ_ABORTED;
        }

        *vp = '\0';
        name = np;

        vp += 2;
        np = strchr(vp, '"');
        *np = '\0';
        value = vp;

        np ++;
        if (*np == ' ') {
            np++;
        }

    	/* need to html encode this stuff */

        sprintf(buffer, "<b>%s:</b> <font face=\"Courier New, Courier, monospace\">%s</font><br>\n", name, value);

        if (net_write(sn->csd, (char *) buffer, strlen(buffer)) == IO_ERROR)
            return REQ_EXIT;
    }

    if (!strcmp(requestMethod, "POST")) {
    	int bytesToRead = atoi(contentLength);
	int bytesActuallyRead = 0;
	char data[bytesToRead];
	char * start = "<font face=\"Courier New, Courier, monospace\">";
	char * end = "</font>";
	
	readPostData(pb, sn, rq, data, bytesToRead, &bytesActuallyRead);
	
        if (net_write(sn->csd, (char *) postData, strlen(postData)) == IO_ERROR)
            return REQ_EXIT;

        if (net_write(sn->csd, start, strlen(start)) == IO_ERROR)
            return REQ_EXIT;

        if (net_write(sn->csd, data, bytesActuallyRead) == IO_ERROR)
            return REQ_EXIT;

        if (net_write(sn->csd, end, strlen(end)) == IO_ERROR)
            return REQ_EXIT;
    }

    if (net_write(sn->csd, (char *) footer, strlen(footer)) == IO_ERROR)
        return REQ_EXIT;

    return REQ_PROCEED;
}

static int readPostData(pblock * pb, Session * sn, Request * rq, 
                        char * io_buf, int readsz, int * bytes)
{
    int ch;
    int inbytes;

    for (inbytes = 0; inbytes < readsz; inbytes++) {
        ch = netbuf_getc(sn->inbuf);
        if (ch == IO_ERROR) {
            protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
            return REQ_ABORTED;
        }
        io_buf[inbytes] = (char) ch;
    }

    *bytes = inbytes;
    return REQ_PROCEED;
}

