/* 								HTBrowse.h
**	DECLARATIONS OF THINGS AVAILABLE FROM HTBrowse.c
**	
**	(c) COPYRIGHT CERN 1994.
**	Please first read the full copyright statement in the file COPYRIGH.
**
**	HTBrowse.c, the main program of a line mode browser,  leaves various
**	public variables atteh disposal of its submodules.
**
**	 6 Oct 92	Nothing provided TO the W3 library (TBL)
*/

#ifndef HTBROWSE_H
#define HTBROWSE_H

#ifdef SHORT_NAMES
#define HTScreenHeight		HTScHeig
#define HTScreenWidth		HTScWidt
#define display_anchors		HTDiAnch
#define interactive		HTIntera
#define reference_mark		HTReMark
#endif

extern  int  HTScreenWidth;		/* By default */
extern  int  HTScreenHeight;		/* Undefined */
extern  BOOL display_anchors;		/* anchor will be shown in text? */

					   
extern char * reference_mark;      	/* Format string for  [1] &c */
extern char * end_mark;      		/* Format string for  [End] */

 
#endif /* HTBROWSE_H */
