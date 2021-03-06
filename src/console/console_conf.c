/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Main configuration file parser for Bareos User Agent
 * some parts may be split into separate files such as
 * the schedule configuration (sch_config.c).
 *
 * Note, the configuration file parser consists of three parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config  scanner in lib/parse_config.c and
 *    lib/parse_config.h. These files contain the parser code,
 *    some utility routines, and the common store routines
 *    (name, int, string).
 *
 * 3. The daemon specific file, which contains the Resource
 *    definitions as well as any specific store routines
 *    for the resource records.
 *
 * Kern Sibbald, January MM, September MM
 */

#include "bareos.h"
#include "console_conf.h"

/* Forward referenced subroutines */


/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
    URES res_all;
}
#else
URES res_all;
#endif

/* Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */

/*  Console "globals" */
static RES_ITEM cons_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_cons.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_cons.hdr.desc), 0, 0, NULL },
   { "rcfile", CFG_TYPE_DIR, ITEM(res_cons.rc_file), 0, 0, NULL },
   { "historyfile", CFG_TYPE_DIR, ITEM(res_cons.hist_file), 0, 0, NULL },
   { "password", CFG_TYPE_MD5PASSWORD, ITEM(res_cons.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "tlsauthenticate",CFG_TYPE_BOOL, ITEM(res_cons.tls_authenticate), 0, 0, NULL },
   { "tlsenable", CFG_TYPE_BOOL, ITEM(res_cons.tls_enable), 0, 0, NULL },
   { "tlsrequire", CFG_TYPE_BOOL, ITEM(res_cons.tls_require), 0, 0, NULL },
   { "tlsverifypeer", CFG_TYPE_BOOL, ITEM(res_cons.tls_verify_peer), 0, CFG_ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", CFG_TYPE_DIR, ITEM(res_cons.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", CFG_TYPE_DIR, ITEM(res_cons.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificaterevocationlist", CFG_TYPE_DIR, ITEM(res_cons.tls_crlfile), 0, 0, NULL },
   { "tlscertificate", CFG_TYPE_DIR, ITEM(res_cons.tls_certfile), 0, 0, NULL },
   { "tlskey", CFG_TYPE_DIR, ITEM(res_cons.tls_keyfile), 0, 0, NULL },
   { "director", CFG_TYPE_STR, ITEM(res_cons.director), 0, 0, NULL },
   { "heartbeatinterval", CFG_TYPE_TIME, ITEM(res_cons.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*  Director's that we can contact */
static RES_ITEM dir_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL },
   { "dirport", CFG_TYPE_PINT32, ITEM(res_dir.DIRport), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "address", CFG_TYPE_STR, ITEM(res_dir.address), 0, 0, NULL },
   { "password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "tlsauthenticate",CFG_TYPE_BOOL, ITEM(res_dir.tls_enable), 0, 0, NULL },
   { "tlsenable", CFG_TYPE_BOOL, ITEM(res_dir.tls_enable), 0, 0, NULL },
   { "tlsrequire", CFG_TYPE_BOOL, ITEM(res_dir.tls_require), 0, 0, NULL },
   { "tlsverifypeer", CFG_TYPE_BOOL, ITEM(res_dir.tls_verify_peer), 0, CFG_ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", CFG_TYPE_DIR, ITEM(res_dir.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", CFG_TYPE_DIR, ITEM(res_dir.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificaterevocationlist", CFG_TYPE_DIR, ITEM(res_dir.tls_crlfile), 0, 0, NULL },
   { "tlscertificate", CFG_TYPE_DIR, ITEM(res_dir.tls_certfile), 0, 0, NULL },
   { "tlskey", CFG_TYPE_DIR, ITEM(res_dir.tls_keyfile), 0, 0, NULL },
   { "heartbeatinterval", CFG_TYPE_TIME, ITEM(res_dir.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 *
 * name items_table resource_code
 */
static RES_TABLE resources[] = {
   { "console", cons_items, R_CONSOLE, sizeof(CONRES) },
   { "director", dir_items, R_DIRECTOR, sizeof(DIRRES) },
   { NULL, NULL, 0, 0 }
};

/* Dump contents of resource */
void dump_resource(CONFIG *config, int type, RES *rres, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)rres;
   bool recurse = true;

   if (res == NULL) {
      printf(_("No record for %d %s\n"), type, config->res_to_str(type));
      return;
   }
   if (type < 0) {                    /* no recursion */
      type = - type;
      recurse = false;
   }
   switch (type) {
   case R_CONSOLE:
      printf(_("Console: name=%s rcfile=%s histfile=%s\n"), rres->name,
             res->res_cons.rc_file, res->res_cons.hist_file);
      break;
   case R_DIRECTOR:
      printf(_("Director: name=%s address=%s DIRport=%d\n"), rres->name,
              res->res_dir.address, res->res_dir.DIRport);
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
   }

   rres = config->GetNextRes(type, rres);
   if (recurse && rres) {
      dump_resource(config, type, rres, sendit, sock);
   }
}

/*
 * Free memory of resource.
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
void free_resource(RES *rres, int type)
{
   URES *res = (URES *)rres;

   if (res == NULL) {
      return;
   }

   /* common stuff -- free the resource name */
   if (res->res_dir.hdr.name) {
      free(res->res_dir.hdr.name);
   }
   if (res->res_dir.hdr.desc) {
      free(res->res_dir.hdr.desc);
   }

   switch (type) {
   case R_CONSOLE:
      if (res->res_cons.rc_file) {
         free(res->res_cons.rc_file);
      }
      if (res->res_cons.hist_file) {
         free(res->res_cons.hist_file);
      }
      if (res->res_cons.tls_ctx) {
         free_tls_context(res->res_cons.tls_ctx);
      }
      if (res->res_cons.tls_ca_certfile) {
         free(res->res_cons.tls_ca_certfile);
      }
      if (res->res_cons.tls_ca_certdir) {
         free(res->res_cons.tls_ca_certdir);
      }
      if (res->res_cons.tls_crlfile) {
         free(res->res_cons.tls_crlfile);
      }
      if (res->res_cons.tls_certfile) {
         free(res->res_cons.tls_certfile);
      }
      if (res->res_cons.tls_keyfile) {
         free(res->res_cons.tls_keyfile);
      }
      break;
   case R_DIRECTOR:
      if (res->res_dir.address) {
         free(res->res_dir.address);
      }
      if (res->res_dir.tls_ctx) {
         free_tls_context(res->res_dir.tls_ctx);
      }
      if (res->res_dir.tls_ca_certfile) {
         free(res->res_dir.tls_ca_certfile);
      }
      if (res->res_dir.tls_ca_certdir) {
         free(res->res_dir.tls_ca_certdir);
      }
      if (res->res_dir.tls_crlfile) {
         free(res->res_dir.tls_crlfile);
      }
      if (res->res_dir.tls_certfile) {
         free(res->res_dir.tls_certfile);
      }
      if (res->res_dir.tls_keyfile) {
         free(res->res_dir.tls_keyfile);
      }
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
   }
   /* Common stuff again -- free the resource, recurse to next one */
   free(res);
}

/* Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
void save_resource(CONFIG *config, int type, RES_ITEM *items, int pass)
{
   int rindex = type - R_FIRST;
   int i;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i=0; items[i].name; i++) {
      if (items[i].flags & CFG_ITEM_REQUIRED) {
            if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {
               Emsg2(M_ABORT, 0, _("%s item is required in %s resource, but not found.\n"),
                 items[i].name, resources[rindex]);
             }
      }
   }

   /* During pass 2, we looked up pointers to all the resources
    * referrenced in the current resource, , now we
    * must copy their address from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      switch (type) {
         /* Resources not containing a resource */
         case R_CONSOLE:
         case R_DIRECTOR:
            break;

         default:
            Emsg1(M_ERROR, 0, _("Unknown resource type %d\n"), type);
            error = 1;
            break;
      }
      /* Note, the resoure name was already saved during pass 1,
       * so here, we can just release it.
       */
      if (res_all.res_dir.hdr.name) {
         free(res_all.res_dir.hdr.name);
         res_all.res_dir.hdr.name = NULL;
      }
      if (res_all.res_dir.hdr.desc) {
         free(res_all.res_dir.hdr.desc);
         res_all.res_dir.hdr.desc = NULL;
      }
      return;
   }

   if (!error) {
      config->insert_resource(rindex, resources[rindex].size);
   }
}

bool parse_cons_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile, NULL, NULL, NULL, NULL, exit_code,
                (void *)&res_all, sizeof(res_all), R_FIRST, R_LAST, resources);
   return config->parse_config();
}
