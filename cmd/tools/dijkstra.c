/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef USE_CGRAPH
#include <cgraph.h>
#else
#include <agraph.h>
#endif
#include <ingraphs.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

#ifndef HUGE
/* HUGE is not defined on 64bit HP-UX */
#define HUGE HUGE_VAL
#endif

static char *CmdName;
static char **Files;
static char **Nodes;
static int setall = 0;		/* if false, don't set dist attribute for
				 * nodes in different components.
				 */
static Agsym_t *len_sym;

typedef struct nodedata_s {
    Agrec_t hdr;
    double dist;		/* always positive for scanned nodes */
} nodedata_t;

typedef struct edgedata_s {
    Agrec_t hdr;
    double length;		/* always positive for initialized */
} edgedata_t;

static double getlength(Agedge_t * e)
{
    edgedata_t *data;
    data = (edgedata_t *) (e->base.data);
    if (data->length == 0) {
	if (len_sym)
	    data->length = atof(agxget(e, len_sym));
	if (data->length <= 0)
	    data->length = 1.0;
    }
    return data->length;
}

#ifdef USE_FNS
static double getdist(Agnode_t * n)
{
    nodedata_t *data;
    data = (nodedata_t *) (n->base.data);
    return data->dist;
}

static void setdist(Agnode_t * n, double dist)
{
    nodedata_t *data;
    data = (nodedata_t *) (n->base.data);
    data->dist = dist;
}
#else
#define getdist(n) (((nodedata_t*)((n)->base.data))->dist)
#define setdist(n,d) (((nodedata_t*)((n)->base.data))->dist = (d))
#endif

static int cmpf(Dt_t * d, void *key1, void *key2, Dtdisc_t * disc)
{
    double t;
    t = getdist((Agnode_t *) key1) - getdist((Agnode_t *) key2);
    if (t < 0)
	return -1;
    if (t > 0)
	return 1;
    if (key1 < key2)
	return -1;
    if (key1 > key2)
	return 1;
    return 0;
}

static Dtdisc_t MyDisc = {
    0,				/* int key */
    0,				/* int size */
    -1,				/* int link */
    0,				/* Dtmake_f makef */
    0,				/* Dtfree_f freef */
    cmpf,			/* Dtcompar_f comparf */
    0,				/* Dthash_f hashf */
    0,				/* Dtmemory_f memoryf */
    0				/* Dtevent_f eventf */
};

static Agnode_t *extract_min(Dict_t * Q)
{
    Agnode_t *rv;
    rv = dtfirst(Q);
    dtdelete(Q, rv);
    return rv;
}

static void update(Dict_t * Q, Agnode_t * dest, Agnode_t * src, double len)
{
    double newlen = getdist(src) + len;
    double oldlen = getdist(dest);

    if (oldlen == 0) {		/* first time to see dest */
	setdist(dest, newlen);
	dtinsert(Q, dest);
    } else if (newlen < oldlen) {
	dtdelete(Q, dest);
	setdist(dest, newlen);
	dtinsert(Q, dest);
    }
}

static void pre(Agraph_t * g)
{
    len_sym = agattr(g, AGEDGE, "len", NULL);
    aginit(g, AGNODE, "dijkstra", sizeof(nodedata_t), 1);
    aginit(g, AGEDGE, "dijkstra", sizeof(edgedata_t), 1);
}

static void post(Agraph_t * g)
{
    Agnode_t *v;
    char buf[256];
    char dflt[256];
    Agsym_t *sym;
    double dist, oldmax;
    double maxdist = 0.0;	/* maximum "finite" distance */

    sym = agattr(g, AGNODE, "dist", "");

    if (setall)
	sprintf(dflt, "%.3lf", HUGE);

#ifdef USE_CGRAPH
    for (v = agfstnode(g); v; v = agnxtnode(g, v)) {
#else
    for (v = agfstnode(g); v; v = agnxtnode(v)) {
#endif
	dist = getdist(v);
	if (dist) {
	    dist--;
	    sprintf(buf, "%.3lf", dist);
	    agxset(v, sym, buf);
	    if (maxdist < dist)
		maxdist = dist;
	} else if (setall)
	    agxset(v, sym, dflt);
    }

    sym = agattrsym(g, "maxdist");
    if (sym) {
	if (!setall) {
	    /* if we are preserving distances in other components,
	     * check previous value of maxdist.
	     */
	    oldmax = atof(agxget(g, sym));
	    if (oldmax > maxdist)
		maxdist = oldmax;
	}
	sprintf(buf, "%.3lf", maxdist);
	agxset(g, sym, buf);
    } else {
	sprintf(buf, "%.3lf", maxdist);
	agattr(g, AGRAPH, "maxdist", buf);
    }

    agclean(g, AGNODE, "dijkstra");
    agclean(g, AGEDGE, "dijkstra");
}

void dijkstra(Dict_t * Q, Agraph_t * G, Agnode_t * n)
{
    Agnode_t *u;
    Agedge_t *e;

    pre(G);
    setdist(n, 1);
    dtinsert(Q, n);
    while ((u = extract_min(Q))) {
#ifdef USE_CGRAPH
	for (e = agfstedge(G, u); e; e = agnxtedge(G, e, u)) {
#else
	for (e = agfstedge(u); e; e = agnxtedge(e, u)) {
#endif
	    update(Q, e->node, u, getlength(e));
	}
    }
    post(G);
}

static char *useString =
    "Usage: dijkstra [-a?] <node> [<file> <node> <file>]\n\
  -a - for nodes in a different component, set dist very large\n\
  -? - print usage\n\
If no files are specified, stdin is used\n";

static void usage(int v)
{
    printf(useString);
    exit(v);
}

static void init(int argc, char *argv[])
{
    int i, j, c;

    CmdName = argv[0];
    while ((c = getopt(argc, argv, ":a?")) != -1) {
	switch (c) {
	case 'a':
	    setall = 1;
	    break;
	case '?':
	    if (optopt == '?')
		usage(0);
	    else
		fprintf(stderr, "%s: option -%c unrecognized - ignored\n",
			CmdName, optopt);
	    break;
	}
    }
    argv += optind;
    argc -= optind;

    if (argc == 0) {
	fprintf(stderr, "%s: no node specified\n", CmdName);
	usage(1);
    }
    Files = malloc(sizeof(char *) * argc / 2 + 2);
    Nodes = malloc(sizeof(char *) * argc / 2 + 2);
    for (j = i = 0; i < argc; i++) {
	Nodes[j] = argv[i++];
	Files[j] = (argv[i] ? argv[i] : "-");
	j++;
    }
    Nodes[j] = Files[j] = 0;
}

static Agraph_t *gread(FILE * fp)
{
    return agread(fp, (Agdisc_t *) 0);
}

int main(int argc, char **argv)
{
    Agraph_t *g;
    Agnode_t *n;
    ingraph_state ig;
    int i = 0;
    int code = 0;
    Dict_t *Q;

    init(argc, argv);
    newIngraph(&ig, Files, gread);

    Q = dtopen(&MyDisc, Dtoset);
    while ((g = nextGraph(&ig)) != 0) {
	dtclear(Q);
	if ((n = agnode(g, Nodes[i], 0)))
	    dijkstra(Q, g, n);
	else {
	    fprintf(stderr, "%s: no node %s in graph %s in %s\n",
		    CmdName, Nodes[i], agnameof(g), fileName(&ig));
	    code = 1;
	}
	agwrite(g, stdout);
	fflush(stdout);
	agclose(g);
	i++;
    }
    exit(code);
}
