/*		Parse HyperText Document Address		HTParse.c
**		================================
**
** history:
**	May 12 94	TAB added as legal char in HTCleanTelnetString
**
*/
#include "tcp.h"
#include "HTUtils.h"
#include "HTParse.h"

#define HEX_ESCAPE '%'

struct struct_parts {
	char * access;		/* Now known as "scheme" */
	char * host;
	char * absolute;
	char * relative;
/*	char * search;		no - treated as part of path */
	char * anchor;
};

/*	Strip white space off a string
**	------------------------------
**
** On exit,
**	Return value points to first non-white character, or to 0 if none.
**	All trailing white space is OVERWRITTEN with zero.
*/

PUBLIC char * HTStrip ARGS1(char *, s)
{
#define SPACE(c) ((c==' ')||(c=='\t')||(c=='\n')) 
    char * p=s;
    if (!s) return NULL;	/* Doesn't dump core if NULL */
    for(p=s;*p;p++);		/* Find end of string */
    for(p--;p>=s;p--) {
    	if(SPACE(*p)) *p=0;	/* Zap trailing blanks */
	else break;
    }
    while(SPACE(*s))s++;	/* Strip leading blanks */
    return s;
}


/*	Scan a filename for its consituents
**	-----------------------------------
**
** On entry,
**	name	points to a document name which may be incomplete.
** On exit,
**      absolute or relative may be nonzero (but not both).
**	host, anchor and access may be nonzero if they were specified.
**	Any which are nonzero point to zero terminated strings.
*/
#ifdef __STDC__
PRIVATE void scan(char * name, struct struct_parts *parts)
#else
PRIVATE void scan(name, parts)
    char * name;
    struct struct_parts *parts;
#endif
{
    char * after_access;
    char * p;
    int length = strlen(name);
    
    parts->access = 0;
    parts->host = 0;
    parts->absolute = 0;
    parts->relative = 0;
    parts->anchor = 0;
    
    after_access = name;
    for(p=name; *p; p++) {
	if (*p==':') {
		*p = 0;
		parts->access = after_access; /* Scheme has been specified */
		after_access = p+1;
		if (0==strcasecomp("URL", parts->access)) {
		    parts->access = NULL;  /* Ignore IETF's URL: pre-prefix */
		} else break;
	}
	if (*p=='/') break;		/* Access has not been specified */
	if (*p=='#') break;
    }
    
    for(p=name+length-1; p>=name; p--) {
	if (*p =='#') {
	    parts->anchor=p+1;
	    *p=0;				/* terminate the rest */
	}
    }
    p = after_access;
    if (*p=='/'){
	if (p[1]=='/') {
	    parts->host = p+2;		/* host has been specified 	*/
	    *p=0;			/* Terminate access 		*/
	    p=strchr(parts->host,'/');	/* look for end of host name if any */
	    if(p) {
	        *p=0;			/* Terminate host */
	        parts->absolute = p+1;		/* Root has been found */
	    }
	} else {
	    parts->absolute = p+1;		/* Root found but no host */
	}	    
    } else {
        parts->relative = (*after_access) ? after_access : 0;	/* zero for "" */
    }

#ifdef OLD_CODE
    /* Access specified but no host: the anchor was not really one
       e.g. news:j462#36487@foo.bar -- JFG 10/jul/92, from bug report */
    /* This kludge doesn't work for example when coming across
       file:/usr/local/www/fred#123
       which loses its anchor. Correct approach in news is to
       escape weird characters not allowed in URL.  TBL 21/dec/93
    */
    if (parts->access && ! parts->host && parts->anchor) {
      *(parts->anchor - 1) = '#';  /* Restore the '#' in the address */
      parts->anchor = 0;
    }
#endif

#ifdef NOT_DEFINED	/* search is just treated as part of path */
    {
        char *p = relative ? relative : absolute;
	if (p) {
	    char * q = strchr(p, '?');	/* Any search string? */
	    if (q) {
	    	*q = 0;			/* If so, chop that off. */
		parts->search = q+1;
	    }
	}
    }
#endif
} /*scan */    


/*	Parse a Name relative to another name
**	-------------------------------------
**
**	This returns those parts of a name which are given (and requested)
**	substituting bits from the related name where necessary.
**
** On entry,
**	aName		A filename given
**      relatedName     A name relative to which aName is to be parsed
**      wanted          A mask for the bits which are wanted.
**
** On exit,
**	returns		A pointer to a malloc'd string which MUST BE FREED
*/
#ifdef __STDC__
char * HTParse(const char * aName, const char * relatedName, int wanted)
#else
char * HTParse(aName, relatedName, wanted)
    char * aName;
    char * relatedName;
    int wanted;
#endif

{
    char * result = 0;
    char * return_value = 0;
    int len;
    char * name = 0;
    char * rel = 0;
    char * p;
    char * access;
    struct struct_parts given, related;
    
    /* Make working copies of input strings to cut up:
    */
    len = strlen(aName)+strlen(relatedName)+10;
    result=(char *)malloc(len);		/* Lots of space: more than enough */
    if (result == NULL) outofmem(__FILE__, "HTParse");
    
    StrAllocCopy(name, aName);
    StrAllocCopy(rel, relatedName);
    
    scan(name, &given);
    scan(rel,  &related); 
    result[0]=0;		/* Clear string  */
    access = given.access ? given.access : related.access;
    if (wanted & PARSE_ACCESS)
        if (access) {
	    strcat(result, access);
	    if(wanted & PARSE_PUNCTUATION) strcat(result, ":");
	}
	
    if (given.access && related.access)	/* If different, inherit nothing. */
        if (strcmp(given.access, related.access)!=0) {
	    related.host=0;
	    related.absolute=0;
	    related.relative=0;
	    related.anchor=0;
	}
	
    if (wanted & PARSE_HOST)
        if(given.host || related.host) {
	    char * tail = result + strlen(result);
	    if(wanted & PARSE_PUNCTUATION) strcat(result, "//");
	    strcat(result, given.host ? given.host : related.host);
#define CLEAN_URLS
#ifdef CLEAN_URLS
	    /* Ignore default port numbers, and trailing dots on FQDNs
	       which will only cause identical adreesses to look different */
	    {
	    	char * p;
		p = strchr(tail, ':');
		if (p && access) {		/* Port specified */
		    if (  (   strcmp(access, "http") == 0
		    	   && strcmp(p, ":80") == 0 )
			||
		          (   strcmp(access, "gopher") == 0
		    	   && strcmp(p, ":70") == 0 )
		    	)
		    *p = (char)0;	/* It is the default: ignore it */
		}
		if (!p) p = tail + strlen(tail); /* After hostname */
		if (*p) {				  /* Henrik 17/04-94 */
		    p--;				/* End of hostname */
		    if (*p == '.') *p = (char)0; /* chop final . */
		}
	    }
#endif
	}
	
    if (given.host && related.host)  /* If different hosts, inherit no path. */
        if (strcmp(given.host, related.host)!=0) {
	    related.absolute=0;
	    related.relative=0;
	    related.anchor=0;
	}
	
    if (wanted & PARSE_PATH) {
        if(given.absolute) {				/* All is given */
	    if(wanted & PARSE_PUNCTUATION) strcat(result, "/");
	    strcat(result, given.absolute);
	} else if(related.absolute) {	/* Adopt path not name */
	    strcat(result, "/");
	    strcat(result, related.absolute);
	    if (given.relative) {
		p = strchr(result, '?');	/* Search part? */
		if (!p) p=result+strlen(result)-1;
		for (; *p!='/'; p--);	/* last / */
		p[1]=0;					/* Remove filename */
		strcat(result, given.relative);		/* Add given one */
		HTSimplify (result);
	    }
	} else if(given.relative) {
	    strcat(result, given.relative);		/* what we've got */
	} else if(related.relative) {
	    strcat(result, related.relative);
	} else {  /* No inheritance */
	    strcat(result, "/");
	}
    }
		
    if (wanted & PARSE_ANCHOR)
        if(given.anchor || related.anchor) {
	    if(wanted & PARSE_PUNCTUATION) strcat(result, "#");
	    strcat(result, given.anchor ? given.anchor : related.anchor);
	}
    free(rel);
    free(name);
    
    StrAllocCopy(return_value, result);
    free(result);
    return return_value;		/* exactly the right length */
}


#if 0  /* NOT USED FOR THE MOMENT */
/*
**	As strcpy() but guaranteed to work correctly
**	with overlapping parameters.	AL 7 Feb 1994
*/
PRIVATE void ari_strcpy ARGS2(char *, to,
			      char *, from)
{
    char * tmp;

    if (!to || !from) return;

    tmp = (char*)malloc(strlen(from)+1);
    if (!tmp) outofmem(__FILE__, "my_strcpy");

    strcpy(tmp, from);
    strcpy(to, tmp);
    free(tmp);
}
#endif


/*	        Simplify a URI
//		--------------
// A URI is allowed to contain the seqeunce xxx/../ which may be
// replaced by "" , and the seqeunce "/./" which may be replaced by "/".
// Simplification helps us recognize duplicate URIs. 
//
//	Thus, 	/etc/junk/../fred 	becomes	/etc/fred
//		/etc/junk/./fred	becomes	/etc/junk/fred
//
//      but we should NOT change
//		http://fred.xxx.edu/../..
//
//	or	../../albert.html
//
// In the same manner, the following prefixed are preserved:
//
//	./<etc>
//	//<etc>
//
// In order to avoid empty URLs the following URLs become:
//
//		/fred/..		becomes /fred/..
//		/fred/././..		becomes /fred/..
//		/fred/.././junk/.././	becomes /fred/..
//
*/
PUBLIC void HTSimplify ARGS1(char *, filename)
{
    int tokcnt = 0;
    char *strptr;
    char *urlptr;
    BOOL prefix = NO;	 /* If prefix == YES then we can delete all segments */
    if (!filename || !*filename)                         /* Just to be sure! */
	return;

    if (TRACE)
	fprintf(stderr, "HTSimplify.. `%s\' ", filename);

    /* Skip prefix, starting ./ and starting ///<etc> */
    if ((urlptr = strstr(filename, "://")) != NULL) {	      /* Find prefix */
	urlptr += 3;
	prefix = YES;
    } else if ((urlptr = strstr(filename, ":/")) != NULL) {
	urlptr += 2;
	prefix = YES;
    } else
	urlptr = filename;
    if (*urlptr == '.' && *(urlptr+1) == '/') {          /* Starting ./<etc> */
	urlptr += 2;
	prefix = YES;
    } else if (*urlptr == '/') {	         /* Some URLs start //<file> */
	while (*++urlptr == '/');
	prefix = YES;
    }
    if (!*urlptr) {					  /* If nothing left */
	if (TRACE)
	    fprintf(stderr, "No simplification possible\n");
	return;
    }

    /* Now we have the string we want to work with */
    strptr = urlptr;
    while (*strptr++) {                        /* Count number of delimiters */
	if (*strptr == '/')
	    tokcnt++;
    }
    {
	BOOL slashtail = NO;
	int segcnt = 0;	     /* Number of 'real segments' (not '.' and '..') */
	char *empty = "";
	char *url = NULL;
	char **tokptr;
	char **tokstart;
	StrAllocCopy(url, urlptr);

	/* Does the URL end with a slash? */
	if(*(filename+strlen(filename)-1) == '/')
	    slashtail = YES;
	
	/* I allocate cnt+2 as I don't know if the url is terminated by '/' */
	if ((tokstart = (char **) calloc(tokcnt+2, sizeof(char *))) == NULL)
	    outofmem(__FILE__, "HTSimplify");

	/* Read the tokens forwards and count `real' segments */
	tokptr = tokstart;
	*tokptr = strtok(url, "/");
	if (strcmp(*tokptr, ".") && strcmp(*tokptr, ".."))
	    segcnt++;
	tokptr++;
	while ((strptr = strtok(NULL, "/")) != NULL) {
	    if (strcmp(strptr, ".") && strcmp(strptr, ".."))
		segcnt++;
	    else if (!strcmp(strptr, "..") && !segcnt)
		prefix = YES;
	    *tokptr++ = strptr;
	}

	/* Scan backwards for '.' and '..' */
	tokptr--;
	while(tokptr >= tokstart) {
	    if (!strcmp(*tokptr, ".")) {
		*tokptr = empty;
	    } else if (!strcmp(*tokptr, "..")) {
		char **pptr = tokptr-1;
		while (pptr >= tokstart) {
		    if (**pptr && strcmp(*pptr, "..") && strcmp(*pptr, ".") &&
			(segcnt > 1 || prefix)) {
			*pptr = empty;
			*tokptr = empty;
			segcnt--;
			break;
		    }
		    pptr--;
		}
	    }
	    tokptr--;
	}

	/* Write the rest out forwards */
	*urlptr = '\0';
	while (*++tokptr) {
	    if (**tokptr) {
		if (*urlptr)		  /* Don't want two in the beginning */
		    strcat(urlptr, "/");
		strcat(urlptr, *tokptr);
	    }
	}

	if (slashtail == YES && *(urlptr+(int)strlen(urlptr)-1) != '/')
	    strcat(urlptr, "/");
	free(url);
	free(tokstart);
    }
    if (TRACE)
	fprintf(stderr, "into\n............ `%s'\n", filename);
}
#ifdef OLD_CODE
    char * p = filename;
    char * q;
    
    if (p) {
	while (*p && (*p == '/' || *p == '.'))     /* Pass starting / or .'s */
	    p++;
	while(*p) {
	    if (*p=='/') {
	    if ((p[1]=='.') && (p[2]=='.') && (p[3]=='/' || !p[3] )) {
		for (q=p-1; (q>=filename) && (*q!='/'); q--); /* prev slash */
		if (q[0]=='/' && 0!=strncmp(q, "/../", 4)
			&&!(q-1>filename && q[-1]=='/')) {
	            ari_strcpy(q, p+3);		/* Remove  /xxx/..	*/
		    if (!*filename) strcpy(filename, "/");
		    p = q-1;		/* Start again with prev slash 	*/
		} else {			/*   xxx/.. leave it!	*/
#ifdef BUG_CODE
		    ari_strcpy(filename, p[3] ? p+4 : p+3); /* rm  xxx/../ */
		    p = filename;		/* Start again */
#endif
		}
	    } else if ((p[1]=='.') && (p[2]=='/' || !p[2])) {
	        ari_strcpy(p, p+2);		/* Remove a slash and a dot */
	    } else if (p[-1] != ':') {
		while (p[1] == '/') {
		    ari_strcpy(p, p+1);	/* Remove multiple slashes */
		}
	    }
	    }
	    p++;
	}  /* end while (*p) */
    } /* end if (p) */
}
#endif /* OLD_CODE */


/*		Make Relative Name
**		------------------
**
** This function creates and returns a string which gives an expression of
** one address as related to another. Where there is no relation, an absolute
** address is retured.
**
**  On entry,
**	Both names must be absolute, fully qualified names of nodes
**	(no anchor bits)
**
**  On exit,
**	The return result points to a newly allocated name which, if
**	parsed by HTParse relative to relatedName, will yield aName.
**	The caller is responsible for freeing the resulting name later.
**
*/
#ifdef __STDC__
char * HTRelative(const char * aName, const char *relatedName)
#else
char * HTRelative(aName, relatedName)
   char * aName;
   char * relatedName;
#endif
{
    char * result = 0;
    CONST char *p = aName;
    CONST char *q = relatedName;
    CONST char * after_access = 0;
    CONST char * path = 0;
    CONST char * last_slash = 0;
    int slashes = 0;
    
    for(;*p; p++, q++) {	/* Find extent of match */
    	if (*p!=*q) break;
	if (*p==':') after_access = p+1;
	if (*p=='/') {
	    last_slash = p;
	    slashes++;
	    if (slashes==3) path=p;
	}
    }
    
    /* q, p point to the first non-matching character or zero */
    
    if (!after_access) {			/* Different access */
        StrAllocCopy(result, aName);
    } else if (slashes<3){			/* Different nodes */
    	StrAllocCopy(result, after_access);
#if 0 /* Henrik */
    } else if (slashes==3){			/* Same node, different path */
        StrAllocCopy(result, path);
#endif
    } else {					/* Some path in common */
        int levels= 0;
        for(; *q && (*q!='#'); q++)  if (*q=='/') levels++;
	result = (char *)malloc(3*levels + strlen(last_slash) + 1);
      if (result == NULL) outofmem(__FILE__, "HTRelative");
	result[0]=0;
	for(;levels; levels--)strcat(result, "../");
	strcat(result, last_slash+1);
    }
    if (TRACE) fprintf(stderr,
		      "HTRelative.. `%s' expressed relative to `%s' is `%s'\n",
		       aName, relatedName, result);
    return result;
}


/*		Escape undesirable characters using %		HTEscape()
**		-------------------------------------
**
**	This function takes a pointer to a string in which
**	some characters may be unacceptable unescaped.
**	It returns a string which has these characters
**	represented by a '%' character followed by two hex digits.
**
**	In the tradition of being conservative in what you do and liberal
**	in what you accept, we encode some characters which in fact are
**	allowed in URLs unencoded -- so DON'T use the table below for
**	parsing! 
**
**	Unlike HTUnEscape(), this routine returns a malloced string.
**
*/

/* Not BOTH static AND const at the same time in gcc :-(, Henrik 18/03-94 
**  code gen error in gcc when making random access to
**  static const table(!!)  */
/* PRIVATE CONST unsigned char isAcceptable[96] = */
PRIVATE unsigned char isAcceptable[96] =

/* Overencodes */
/*	Bit 0		xalpha		-- see HTFile.h
**	Bit 1		xpalpha		-- as xalpha but with plus.
**	Bit 2 ...	path		-- as xpalpha but with /
*/
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,7,6,0,7,7,4,	/* 2x   !"#$%&'()*+,-./	 */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,	/* 3x  0123456789:;<=>?	 */
	 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 4x  @ABCDEFGHIJKLMNO  */
	 7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,	/* 5X  PQRSTUVWXYZ[\]^_	 */
	 0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 6x  `abcdefghijklmno	 */
	 7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0 };	/* 7X  pqrstuvwxyz{\}~	DEL */

PRIVATE char *hex = "0123456789ABCDEF";

PUBLIC char * HTEscape ARGS2 (CONST char *, str,
	unsigned char, mask)
{
#define ACCEPTABLE(a)	( a>=32 && a<128 && ((isAcceptable[a-32]) & mask))
    CONST char * p;
    char * q;
    char * result;
    int unacceptable = 0;
    for(p=str; *p; p++)
        if (!ACCEPTABLE((unsigned char)TOASCII(*p)))
		unacceptable++;
    result = (char *) malloc(p-str + unacceptable+ unacceptable + 1);
    if (result == NULL) outofmem(__FILE__, "HTEscape");
    for(q=result, p=str; *p; p++) {
    	unsigned char a = TOASCII(*p);
	if (!ACCEPTABLE(a)) {
	    *q++ = HEX_ESCAPE;	/* Means hex commming */
	    *q++ = hex[a >> 4];
	    *q++ = hex[a & 15];
	}
	else *q++ = *p;
    }
    *q++ = 0;			/* Terminate */
    return result;
}


/*		Decode %xx escaped characters			HTUnEscape()
**		-----------------------------
**
**	This function takes a pointer to a string in which some
**	characters may have been encoded in %xy form, where xy is
**	the acsii hex code for character 16x+y.
**	The string is converted in place, as it will never grow.
*/

PRIVATE char from_hex ARGS1(char, c)
{
    return  c >= '0' && c <= '9' ?  c - '0' 
    	    : c >= 'A' && c <= 'F'? c - 'A' + 10
    	    : c - 'a' + 10;	/* accept small letters just in case */
}

PUBLIC char * HTUnEscape ARGS1( char *, str)
{
    char * p = str;
    char * q = str;

    if (!str) {					      /* Just for safety ;-) */
	if (TRACE)
	    fprintf(stderr, "HTUnEscape.. Called with NULL argument.\n");
	return "";
    }
    while(*p) {
        if (*p == HEX_ESCAPE) {
	    p++;
	    if (*p) *q = from_hex(*p++) * 16;
	    if (*p) *q = FROMASCII(*q + from_hex(*p++));
	    q++;
	} else {
	    *q++ = *p++; 
	}
    }
    
    *q++ = 0;
    return str;
    
} /* HTUnEscape */


/*							HTCleanTelnetString()
 *	Make sure that the given string doesn't contain characters that
 *	could cause security holes, such as newlines in ftp, gopher,
 *	news or telnet URLs; more specifically: allows everything between
 *	ASCII 20-7E, and also A0-FE, inclusive. Also TAB ('\t') allowed!
 *
 * On entry,
 *	str	the string that is *modified* if necessary.  The
 *		string will be truncated at the first illegal
 *		character that is encountered.
 * On exit,
 *	returns	YES, if the string was modified.
 *		NO, otherwise.
 */
PUBLIC BOOL HTCleanTelnetString ARGS1(char *, str)
{
    char * cur = str;

    if (!str) return NO;

    while (*cur) {
	int a = TOASCII(*cur);
	if (a != 0x9 && (a < 0x20 || (a > 0x7E && a < 0xA0) ||  a > 0xFE)) {
	    CTRACE(stderr, "Illegal..... character in URL: \"%s\"\n",str);
	    *cur = 0;
	    CTRACE(stderr, "Truncated... \"%s\"\n",str);
	    return YES;
	}
	cur++;
    }
    return NO;
}

