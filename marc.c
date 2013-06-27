/*
 * $Header: /cvsroot/arc/arc/marc.c,v 1.6 2010/08/07 13:06:11 k_reimer Exp $
 */

/*  MARC - Archive merge utility

    Version 5.21, created on 04/22/87 at 15:05:10

(C) COPYRIGHT 1985-87 by System Enhancement Associates; ALL RIGHTS RESERVED

    By:	 Thom Henderson

    Description:
	 This program is used to "merge" archives.  That is, to move
	 files from one archive to another with no data conversion.
	 Please refer to the ARC source for a description of archives
	 and archive formats.

    Instructions:
	 Run this program with no arguments for complete instructions.

    Language:
	 Computer Innovations Optimizing C86
*/
#include <stdio.h>
#include <string.h>
#include "arc.h"

#if	UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "proto.h"

#ifndef	__STDC__
char *calloc(), *malloc(), *realloc(); /* memory managers */
#endif
int gethdr(), match(), readhdr();
VOID	arcdie(), filecopy(), setstamp(), writehdr();
static VOID copyfile(), expandlst(), merge();

FILE *src;			       /* source archive */
char srcname[STRLEN];		       /* source archive name */
char *pinbuf;				/* block input buffer */

static char **lst;		       /* files list */
static int lnum;		       /* length of files list */

int
main(nargs,arg)			       /* system entry point */
int nargs;			       /* number of arguments */
char *arg[];			       /* pointers to arguments */
{
    char *makefnam();
    char *envfind();
#if	!_MTS
    char *arctemp2;		       /* temp file stuff */
#endif
#if	GEMDOS
    VOID exitpause();
#endif
    int n;			       /* index */
#if	UNIX
    struct	stat	sbuf;
#endif


    if(nargs<3)
    {	 printf("MARC - Archive merger, Version 5.21p, created on 08/07/2010\n");
/*	 printf("(C) COPYRIGHT 1985,86,87 by System Enhancement Associates\n");
	 printf("Please refer all inquiries to:\n\n");
	 printf("	System Enhancement Associates\n");
	 printf("	21 New Street, Wayne NJ 07470\n\n");
	 printf("You may copy and distribute this program freely,");
	 printf(" under the terms of the General Public License.\n");
*/
	 printf("Usage: MARC <tgtarc> <srcarc> [<filename> . . .]\n");
	 printf("Where: <tgtarc> is the archive to add files to,\n");
	 printf("	<srcarc> is the archive to get files from, and\n");
	 printf("	<filename> is zero or more file names to get.\n");
	 printf("\nAdapted from MSDOS by Howard Chu\n");
#if	GEMDOS
	 exitpause();
#endif
	 return 1;
    }

	/* see where temp files go */
#if	!_MTS
	arctemp = calloc(1, STRLEN);
	if (!(arctemp2 = envfind("ARCTEMP")))
		arctemp2 = envfind("TMPDIR");
	if (arctemp2) {
		strncpy(arctemp, arctemp2, STRLEN - 16);
		arctemp[STRLEN - 17] = '\0';
		n = strlen(arctemp);
		if (arctemp[n - 1] != CUTOFF)
			arctemp[n] = CUTOFF;
	}
#if	UNIX
	else	strcpy(arctemp, "/tmp/");
#endif
#if	!MSDOS
	{
		strcat(arctemp, "AXXXXXX");
		int fd = mkstemp(arctemp);
		if (fd == -1)
		{
			fprintf(stderr, "Unable to create temporary files: %s\n", strerror(errno));
			exit(235);
		}
		close(fd);
		unlink(arctemp);
	}
#else
	strcat(arctemp, "$ARCTEMP");
#endif
#else
	guinfo("SHFSEP	", gotinf);
	sepchr[0] = gotinf[0];
	guinfo("SCRFCHAR", gotinf);
	tmpchr[0] = gotinf[0];
	arctemp = "-$$$";
	arctemp[0] = tmpchr[0];
#endif

	for (n = 1; n < nargs; n++)
	  if (strlen(arg[n]) > (STRLEN - 16))
	  {
	    fprintf(stderr, "Too long argument: %s\n", arg[n]);
	    exit(235);
	  }

#if	UNIX
	if (!stat(arg[1],&sbuf))
		strcpy(arcname,arg[1]);
	else
		makefnam(arg[1],".arc",arcname);
	if (!stat(arg[2],&sbuf))
		strcpy(srcname,arg[2]);
	else
		makefnam(arg[2],".arc",srcname);
#else
    makefnam(arg[1],".ARC",arcname);   /* fix up archive names */
    makefnam(arg[2],".ARC",srcname);
/*	makefnam(".$$$",arcname,newname);*/
#endif
	sprintf(newname,"%s.arc",arctemp);

    arc = fopen(arcname,OPEN_R);	       /* open the archives */
    if(!(src=fopen(srcname,OPEN_R)))
	 arcdie("Cannot read source archive %s",srcname);
    if(!(new=tmpopen(newname)))
	 arcdie("Cannot create new archive %s",newname);

    if(!arc)
	 printf("Creating new archive %s\n",arcname);

    /* get the files list set up */

    lnum = nargs-3;		       /* initial length of list */
    if(lnum<1)			       /* phoney for default case */
    {	 lnum = 1;
	 lst = (char **) calloc(1,sizeof(char *));
	 lst[0] = "*.*";
    }
    else			       /* else use filenames given */
    {	 lst = (char **) calloc(lnum,sizeof(char *));
	 for(n=3; n<nargs; n++)
	      lst[n-3] = arg[n];

	 for(n=0; n<lnum; )	       /* expand indirect references */
	 {    if(*lst[n] == '@')
		   expandlst(n);
	      else n++;
	 }
    }
    if (!(pinbuf = malloc(MYBUF)))
	arcdie("Not enough memory for input buffer.");

    merge(lnum,lst);		       /* merge desired files */

    if(arc) fclose(arc);	       /* close the archives */
    fclose(src);
    fclose(new);

    if(arc)			       /* make the switch */
	 if(unlink(arcname))
	      arcdie("Unable to delete old copy of %s",arcname);
    if(move(newname,arcname))
	 arcdie("Unable to rename %s to %s",newname,arcname);

    setstamp(arcname,arcdate,arctime);     /* new arc matches newest file */

#if	GEMDOS
    exitpause();
#endif
    return nerrs;
}

static	VOID
merge(nargs,arg)		       /* merge two archives */
int nargs;			       /* number of filename templates */
char *arg[];			       /* pointers to names */
{
    struct heads srch;		       /* source archive header */
    struct heads arch;		       /* target archive header */
    int gotsrc, gotarc;		       /* archive entry versions (0=end) */
    int copy;			       /* true to copy file from source */
    int n;			       /* index */

    gotsrc = gethdr(src,&srch);	       /* get first source file */
    gotarc = gethdr(arc,&arch);	       /* get first target file */

    while(gotsrc || gotarc)	       /* while more to merge */
    {	 if(strcmp(srch.name,arch.name)>0)
	 {    copyfile(arc,&arch,gotarc);
	      gotarc = gethdr(arc,&arch);
	 }

	 else if(strcmp(srch.name,arch.name)<0)
	 {    copy = 0;
	      for(n=0; n<nargs; n++)
	      {	   if(match(srch.name,arg[n]))
		   {	copy = 1;
			break;
		   }
	      }
	      if(copy)		       /* select source or target */
	      {	   printf("Adding file:	  %s\n",srch.name);
		   copyfile(src,&srch,gotsrc);
	      }
	      else fseek(src,srch.size,1);
	      gotsrc = gethdr(src,&srch);
	 }

	 else			       /* duplicate names */
	 {    copy = 0;
	      {	   if((srch.date>arch.date)
		   || (srch.date==arch.date && srch.time>arch.time))
		   {	for(n=0; n<nargs; n++)
			{    if(match(srch.name,arg[n]))
			     {	  copy = 1;
				  break;
			     }
			}
		   }
	      }
	      if(copy)		       /* select source or target */
	      {	   printf("Updating file: %s\n",srch.name);
		   copyfile(src,&srch,gotsrc);
		   gotsrc = gethdr(src,&srch);
		   if(gotarc)
		   {	fseek(arc,arch.size,1);
			gotarc = gethdr(arc,&arch);
		   }
	      }
	      else
	      {	   copyfile(arc,&arch,gotarc);
		   gotarc = gethdr(arc,&arch);
		   if(gotsrc)
		   {	fseek(src,srch.size,1);
			gotsrc = gethdr(src,&srch);
		   }
	      }
	 }
    }

    hdrver = 0;			       /* end of archive marker */
    writehdr(&arch,new);	       /* mark the end of the archive */
}

int gethdr(f,hdr)		       /* special read header for merge */
FILE *f;			       /* file to read from */
struct heads *hdr;		       /* storage for header */
{
    char *i = hdr->name;	       /* string index */
    int n;			       /* index */

    for(n=0; n<FNLEN; n++)	       /* fill name field */
	 *i++ = 0176;		       /* impossible high value */
    *--i = '\0';		       /* properly end the name */

    hdrver = 0;			       /* reset header version */
    if(readhdr(hdr,f))		       /* use normal reading logic */
	 return hdrver;		       /* return the version */
    else return 0;		       /* or fake end of archive */
}

static VOID
copyfile(f,hdr,ver)		       /* copy a file from an archive */
FILE *f;			       /* archive to copy from */
struct heads *hdr;		       /* header data for file */
int ver;			       /* header version */
{
    hdrver = ver;		       /* set header version */
    writehdr(hdr,new);		       /* write out the header */
    filecopy(f,new,hdr->size);	       /* copy over the data */
}

static VOID
expandlst(n)			       /* expand an indirect reference */
int n;				       /* number of entry to expand */
{
    FILE *lf, *fopen();		       /* list file, opener */
    char buf[100];		       /* input buffer */
    int x;			       /* index */
    char *p = lst[n]+1;		       /* filename pointer */
    char *makefnam();

    if(*p)			       /* use name if one was given */
    {	 makefnam(p,".CMD",buf);
	 upper(buf);
	 if(!(lf=fopen(buf,"r")))
	      arcdie("Cannot read list of files in %s",buf);
    }
    else lf = stdin;		       /* else use standard input */

    for(x=n+1; x<lnum; x++)	       /* drop reference from the list */
	 lst[x-1] = lst[x];
    lnum--;

    while(fscanf(lf,"%99s",buf)>0)     /* read in the list */
    {	 if(!(lst=(char **) realloc(lst,(lnum+1)*sizeof(char *))))
	      arcdie("too many file references");

	 lst[lnum] = malloc(strlen(buf)+1);
	 strcpy(lst[lnum],buf);	       /* save the name */
	 lnum++;
    }

    if(lf!=stdin)		       /* avoid closing standard input */
	 fclose(lf);
}
