/* N O T E S - to be removed before release
executable
l:\projects\libwww\www\linemode\pics\windows\windebug\wwwpics.exe
working dir
l:\projects\libwww\www\library\implementation\windows\windebug
args

dlls
l:\projects\libwww\www\pics\implementation\windows\windebug\wwwpics.dll

switch to HTStrMatch
*/

#include <stdio.h>
#include <string.h>
#include "WWWLib.h"
#include "WWWApp.h"
#include "HTHeader.h"
#include "CSLApp.h"
#include "CSParse.h"
#include "CSLUtils.h"
#include "CSLL.h"
#include "CSUser.h"
#include "CSUserSt.h"
#include "CSUsrLst.h"

/* ReqParms - list of pending HTRequests and their callbacks an' stuff */
typedef struct {
    HTRequest * pReq;
    HTRequest * pBureauReq;

    CSDisposition_criteria criteria;/* if the criteria are met */
    CSDisposition_callback * pCallback;	/*  call pCallback */
    CSError_t disposition;	/*   with */
    CSLabel_t * pCSLabel;	/*   all */
    CSUser_t * pCSUser;		/*   these */
    void * pVoid;		/*   parameters */
    } ReqParms_t;

ReqParms_t DefaultReqParms = {0, 0, CSApp_neverCall, 0, CSError_APP, 0, 0, 0};

/* handy strings */

PRIVATE char * S_mimeLabel = "PICS-Label*";
PRIVATE char * S_machRead = "application/x-pics-machine-readable";
PRIVATE char * S_label = "application/pics-label";
PRIVATE char * S_user = "application/x-pics-user";
PRIVATE char * S_URLList = "application/x-url-list";

/* LoadURLToConverter - load a URL and set the output to go to converter.
                        Useful for loading user list and profiles. */
HTList * ListWithBureauBefore = 0;
HTList * ListWithBureauAfter = 0;
PRIVATE BOOL LoadURLToConverter(const char * url, const char * relatedName, 
				const char * type, HTConverter * converter, 
				const char * errMessage, HTRequest ** pPReq, 
				HTList * befores, HTList * afters)
{
    BOOL ret;
    char * fullURL;
    HTRequest * pRequest;
    HTParentAnchor * pParentAnchor;
    HTList * conversions = HTList_new();

    pRequest = HTRequest_new();
    fullURL = HTParse(url, relatedName, PARSE_ALL);
    pParentAnchor = (HTParentAnchor *) HTAnchor_findAddress(fullURL);
    HTRequest_setPreemptive(pRequest, YES);
    if (converter) {
        HTConversion_add(conversions, type, "*/*", converter, 1.0, 0.0, 0.0);
	HTRequest_setConversion(pRequest, conversions, YES);
    }
    if (pPReq)
        *pPReq = pRequest;
    HTRequest_setBefore(pRequest, ListWithBureauBefore, YES);
    HTRequest_setAfter(pRequest, ListWithBureauAfter, YES);
    if ((ret = HTLoadAnchor((HTAnchor *) pParentAnchor, pRequest)) != YES)
        HTTrace("PICS: Can't access %s.\n", errMessage);
    if (converter)
        HTConversion_deleteAll(conversions);
    HTRequest_delete(pRequest);
    HT_FREE(fullURL);
    return ret;
}

/* This should be in CSUserList, but that would require 
   that LoadURLToConverter and S_URLList be PUBLIC and 
   added to some header, which one? */
PUBLIC BOOL CSUserList_load(char * url, char * relatedName)
{
    return LoadURLToConverter(url, relatedName, S_URLList, CSUserLists, 
			      "PICS user list", 0, 0, 0);
}

/* L O A D E D U S E R */
/* LoadedUsers is a list of all the users who have 
   had their descritpion files loaded */
typedef struct {
    CSUser_t * pCSUser;
    char *     url;
} CSLoadedUser_t;
PRIVATE HTList * LoadedUsers = 0;

PRIVATE CSLoadedUser_t * CSLoadedUser_new(CSUser_t * pCSUser, char * url)
{
    CSLoadedUser_t * pCSLoadedUser;
    if ((pCSLoadedUser = (CSLoadedUser_t *)HT_MALLOC(sizeof(CSLoadedUser_t))) == NULL)
        HT_OUTOFMEM("CSLoadedUser_t");
    pCSLoadedUser->pCSUser = pCSUser;
    pCSLoadedUser->url = 0;
    StrAllocCopy(pCSLoadedUser->url, url);
    return pCSLoadedUser;
}

PRIVATE void CSLoadedUser_delete(CSLoadedUser_t * pCSLoadedUser)
{
    CSUser_free(pCSLoadedUser->pCSUser);
    HT_FREE(pCSLoadedUser->url);
    HT_FREE(pCSLoadedUser);
}

PRIVATE CSLoadedUser_t * CSLoadedUser_findUser(CSUser_t * pCSUser)
{
    HTList * cur = LoadedUsers;
    CSLoadedUser_t * pCSLoadedUser;
    while ((pCSLoadedUser = (CSLoadedUser_t *) HTList_nextObject(cur))) {
        if (pCSLoadedUser->pCSUser ==  pCSUser)
            return pCSLoadedUser;
    }
    return 0;
}

PRIVATE CSLoadedUser_t * CSLoadedUser_findName(char * name)
{
    HTList * cur = LoadedUsers;
    CSLoadedUser_t * pCSLoadedUser;
    while ((pCSLoadedUser = (CSLoadedUser_t *) HTList_nextObject(cur))) {
        if (!strcasecomp(CSUser_name(pCSLoadedUser->pCSUser), name))
            return pCSLoadedUser;
    }
    return 0;
}

PUBLIC BOOL CSLoadedUser_remove(CSUser_t * pCSUser)
{
    CSLoadedUser_t * pCSLoadedUser;
    if (!(pCSLoadedUser = CSLoadedUser_findUser(pCSUser)))
        return NO;
    HTList_removeObject(LoadedUsers, (void *)pCSLoadedUser);
    CSLoadedUser_delete(pCSLoadedUser);
    return YES;
}

/* PUBLIC version of findName */
PUBLIC BOOL CSLoadedUser_find(char * name)
{
    return (CSLoadedUser_findName(name) != 0);
}

PRIVATE CSLoadedUser_t * CSLoadedUser_findUrl(char * url)
{
    HTList * cur = LoadedUsers;
    CSLoadedUser_t * pCSLoadedUser;
    while ((pCSLoadedUser = (CSLoadedUser_t *) HTList_nextObject(cur))) {
        if (!strcasecomp(pCSLoadedUser->url, url))
            return pCSLoadedUser;
    }
    return 0;
}

CSApp_userCallback * PUserCallback = 0;

PUBLIC BOOL CSLoadedUser_add(CSUser_t * pCSUser, char * url)
{
    int callBackRet;
    CSLoadedUser_t * pCSLoadedUser;
    callBackRet = PUserCallback(pCSUser, DefaultReqParms.pVoid);
    switch (callBackRet) {
        case 1:
            DefaultReqParms.pCSUser = pCSUser;
	    if (PICS_TRACE) HTTrace("PICS: Setting default user to \"%s\".\n", CSUser_name(pCSUser));
            break;
	case 0:
	    if (PICS_TRACE) HTTrace("PICS: Loaded user \"%s\".\n", CSUser_name(pCSUser));
	    break;
	case -1:
	    if (PICS_TRACE) HTTrace("PICS: User \"%s\" not loaded.\n", CSUser_name(pCSUser));
	    return YES;
	default:
	    if (PICS_TRACE) HTTrace("PICS: Aborting load user \"%s\".\n", CSUser_name(pCSUser));
	    return NO;
    }
    if ((pCSLoadedUser = CSLoadedUser_findName(CSUser_name(pCSUser)))) {
        HTTrace("PICS: Replacing previous user \"%s\".\n", 
		CSUser_name(pCSUser));
	HTList_removeObject(LoadedUsers, (void *)pCSLoadedUser);
    }
    pCSLoadedUser = CSLoadedUser_new(pCSUser, url);
    return (HTList_addObject(LoadedUsers, (void *)pCSLoadedUser));
}

PUBLIC BOOL CSLoadedUser_deleteAll (void)
{
    CSLoadedUser_t * pCSLoadedUser;

    while ((pCSLoadedUser = (CSLoadedUser_t *) HTList_removeLastObject(LoadedUsers))) {
        CSLoadedUser_delete(pCSLoadedUser);
    }
    HTList_delete(LoadedUsers);
    LoadedUsers = NULL;
    return YES;
}

PUBLIC int CSLoadedUser_enum(CSLoadedUserCallback * pCallback, void * pVoid)
{
    HTList * cur = LoadedUsers;
    CSLoadedUser_t * pCSLoadedUser;
    int retVal, i = 0;
    while ((pCSLoadedUser = (CSLoadedUser_t *) HTList_nextObject(cur))) {
        if ((retVal = (*pCallback)(pCSLoadedUser->pCSUser, i, pVoid)) != CSError_OK)
            return (retVal);
        i++;
    }
    return 0;
}

PUBLIC CSUser_t * CSLoadedUser_load(char * url, char * relatedName)
{
    BOOL err = 0;
    char * fullURL = HTParse(url, relatedName, PARSE_ALL);
    CSLoadedUser_t * pCSLoadedUser;
    if (!LoadURLToConverter(fullURL, 0, 0, 0, "PICS user file", 0, 0, 0))
/*    if (!LoadURLToConverter(fullURL, 0, S_user, CSParseUser, 
			    "PICS user file", 0, 0, 0)) */
        err = 1;
    else if ((pCSLoadedUser = CSLoadedUser_findUrl(fullURL)) == NULL)
        err = 1;
    HT_FREE(fullURL);
   return err ? 0 : pCSLoadedUser->pCSUser;
}

/* R E Q P A R M S */
HTList * ReqParms = 0;

PRIVATE ReqParms_t * ReqParms_new(HTRequest * pReq, CSUser_t * pCSUser, CSDisposition_callback * pCallback, CSDisposition_criteria criteria, void * pVoid)
{
    ReqParms_t * me;
    if ((me = (ReqParms_t *) HT_CALLOC(1, sizeof(ReqParms_t))) == NULL)
        HT_OUTOFMEM("ReqParms_t");
    me->pReq = pReq;
    me->disposition = CSError_APP;
    me->pBureauReq = 0;
    me->pCSUser = pCSUser;
    me->pCallback = pCallback;
    me->criteria = criteria;
    me->pVoid = pVoid;
    return me;
}

PRIVATE void ReqParms_free(ReqParms_t * pReqParms)
{
    HT_FREE(pReqParms);
}

PRIVATE ReqParms_t * ReqParms_getReq(HTRequest * pReq)
{
    HTList * cur = ReqParms;
    ReqParms_t * pReqParms;
    while ((pReqParms = (ReqParms_t *) HTList_nextObject(cur)))
        if (pReqParms->pReq == pReq)
            return pReqParms;
    return 0;
}

PRIVATE ReqParms_t * ReqParms_getBureauReq(HTRequest * pBureauReq)
{
    HTList * cur = ReqParms;
    ReqParms_t * pReqParms;
    while ((pReqParms = (ReqParms_t *) HTList_nextObject(cur)))
        if (pReqParms->pBureauReq == pBureauReq)
            return pReqParms;
    return 0;
}

PRIVATE BOOL ReqParms_cache(ReqParms_t * pReqParms, CSLabel_t * pCSLabel)
{
    return NO;
}

PRIVATE BOOL ReqParms_checkCache(ReqParms_t * pReqParms, int * pRet)
{
    return NO;
}

PUBLIC BOOL CSApp_label(HTRequest * pReq, CSLabel_t * pCSLabel)
{
    ReqParms_t * pReqParms;
    if (!(pReqParms = ReqParms_getBureauReq(pReq)))
        pReqParms = &DefaultReqParms;
    if (pReqParms->pBureauReq != pReq) {
        HTTrace("PICS: Could not find original request\n");
	return NO;
    }
    pReqParms->pCSLabel = pCSLabel;
    pReqParms->disposition = CSCheckLabel_checkLabelAndUser(pCSLabel, 
							   pReqParms->pCSUser);
    ReqParms_cache(pReqParms, pCSLabel);
    return YES;
}

PRIVATE int ReqParms_checkDisposition(ReqParms_t * pReqParms)
{
    int kill;
    kill = pReqParms->disposition != CSError_OK;
    if ((kill && pReqParms->criteria & CSApp_callOnBad) || 
	(!kill && pReqParms->criteria & CSApp_callOnGood))
        pReqParms->disposition = (*pReqParms->pCallback)(pReqParms->pReq, 
							 pReqParms->pCSLabel, 
							 pReqParms->pCSUser, 
						       pReqParms->disposition, 
							 pReqParms->pVoid);
    return (pReqParms->disposition == CSError_OK ? HT_OK : HT_INTERRUPTED);
    CSLabel_free(pReqParms->pCSLabel);
    if (pReqParms->disposition == CSError_OK)
	return HT_OK;
    return HT_OK;
}

PRIVATE BOOL ReqParms_removeRequest(ReqParms_t * pReqParms)
{
    HTList_removeObject(ReqParms, (void *)pReqParms);
    ReqParms_free(pReqParms);
    return YES;
}
/*
PRIVATE BOOL ReqParms_removeRequest(HTRequest * pReq)
{
    ReqParms_t * pReqParms;
    if ((pReqParms = ReqParms_getReq(pReq)))
        return HTList_removeObject(ReqParms, (void *)pReqParms);
    return NO;
}

PRIVATE CSUser_t * ReqParms_getUser(HTRequest * pReq)
{
    ReqParms_t * pReqParms;
    if ((pReqParms = ReqParms_getReq(pReq)))
        return pReqParms->pCSUser;
    return 0;
}
*/

/* U S E R */
PUBLIC BOOL CSApp_registerDefaultUserByName(char * user, char * password)
{
    CSUser_t * pCSUser;
    CSLoadedUser_t * pCSLoadedUser;
    if (!(pCSLoadedUser = CSLoadedUser_findName(user))) {
        if (PICS_TRACE) HTTrace("PICS: User \"%s\" not found.\n", 
				CSUser_name(pCSUser));
        return NO;
    }
    pCSUser = pCSLoadedUser->pCSUser;
    if (CSUser_checkPassword(pCSUser, password) == NO) {
        HTTrace("PICS: Bad password for user \"%s\".\n", CSUser_name(pCSUser));
        return NO;
    }
    DefaultReqParms.pCSUser = pCSUser;
    return YES;
}

PUBLIC CSUser_t * CSApp_registerUserByName(char* user, char* password)
{
    CSLoadedUser_t * pCSLoadedUser;
    if (!(pCSLoadedUser = CSLoadedUser_findName(user)))
        return 0;
    if (CSUser_checkPassword(pCSLoadedUser->pCSUser, password) == NO) {
        HTTrace("PICS: Bad password for user \"%s\"\n", 
		CSUser_name(pCSLoadedUser->pCSUser));
        return 0;
    }
    return pCSLoadedUser->pCSUser;
}

PUBLIC BOOL CSApp_setDefaultUser(CSUser_t * pCSUser)
{
    if (!pCSUser)
        return NO;
    DefaultReqParms.pCSUser = pCSUser;
    return YES;
}

PUBLIC BOOL CSApp_checkHUser(CSUser_t * pCSUser)
{
    return YES;
}

PUBLIC BOOL CSApp_unregisterDefaultUser(void)
{
    DefaultReqParms.pCSUser = 0;
    return YES;
}

PUBLIC BOOL CSApp_unregisterUser(CSUser_t * pCSUser)
{
    if (DefaultReqParms.pCSUser == pCSUser)
        DefaultReqParms.pCSUser = 0;
    return CSLoadedUser_remove(pCSUser);
}

/* base of all streams */
#define PUTBLOCK(b, l)	(*target->isa->put_block)(target, b, l)
struct _HTStream {
    const HTStreamClass *	isa;
    /* ... */
};

/* HTNetCallbacks */
PRIVATE int CSApp_netBefore (HTRequest * pReq, void * param, int status)
{
    ReqParms_t * pReqParms;
    char * bureau;
    char * ptr;
    char * url;
    int ret;

    if (ReqParms_getBureauReq(pReq) || 
	pReq == DefaultReqParms.pBureauReq)
        return HT_OK;
    if (!(pReqParms = ReqParms_getReq(pReq))) {
        pReqParms = &DefaultReqParms;
	DefaultReqParms.pReq = pReq;
    }
    if (!pReqParms->pCSUser)			/* PICS not set up */
        return HT_OK;
    if (!(bureau = CSUser_bureau(pReqParms->pCSUser)))
        return HT_OK;				/* Header will handle it */
    url = HTAnchor_address((HTAnchor*)HTRequest_anchor(pReq));
    if (ReqParms_checkCache(pReqParms, &ret))
        return ret;
    ptr = CSUser_getLabels(pReqParms->pCSUser, url, CSOption_normal, 
			   CSCompleteness_full);
    if (PICS_TRACE) HTTrace("PICS: label request:\n%s\n", ptr);
    /* get label and set disposition */
    if (!(LoadURLToConverter(ptr, 0, S_label, CSParseLabel, "Label bureau", 
			     &pReqParms->pBureauReq, ListWithBureauBefore, 
			     ListWithBureauAfter))) {
        HT_FREE(ptr);
	HTTrace("PICS: Couldn't load labels for \"%s\" at bureau \"%s\".\n", url, bureau);
	return HT_OK;
    }
    HT_FREE(ptr);
    if (pReqParms->disposition == CSError_APP) {
        HTTrace("PICS: No labels for \"%s\" at bureau \"%s\".\n", url, bureau);
	return HT_OK;
    }
    return ReqParms_checkDisposition(pReqParms);
}

PRIVATE int CSApp_netAfter (HTRequest * pReq, void * param, int status)
{
    ReqParms_t * pReqParms;

    if (!(pReqParms = ReqParms_getReq(pReq)))
        pReqParms = &DefaultReqParms;

    if (pReqParms == &DefaultReqParms)
        pReqParms->pReq = 0;
    else
        ReqParms_removeRequest(pReqParms);

    return HT_OK;
}

/* will we need this? */
PRIVATE int CSApp_bureauBefore (HTRequest * pReq, int status)
{
    ReqParms_t * pReqParms;
    if (!(pReqParms = ReqParms_getBureauReq(pReq)))
        pReqParms = &DefaultReqParms;
    if (pReq != pReqParms->pBureauReq)
        return HT_ERROR;
    return HT_OK;
}

PRIVATE int CSApp_bureauAfter (HTRequest * pReq, int status)
{
    ReqParms_t * pReqParms;
    if (!(pReqParms = ReqParms_getBureauReq(pReq)))
        pReqParms = &DefaultReqParms;
    if ((pReq != pReqParms->pBureauReq))
        return HT_ERROR;
    if (PICS_TRACE) HTTrace("PICS: Load was %sOK\n", 
			    pReqParms->disposition == CSError_OK ? "" : "!");
    return HT_OK;
}

/*HTPostCallback CSApp_headerGenerator;*/
PRIVATE int CSApp_headerGenerator (HTRequest * pReq, HTStream * target)
{
    char * translated;
    HTParentAnchor *anchor = HTRequest_anchor(pReq);
    char * url;
    ReqParms_t * pReqParms = ReqParms_getReq(pReq);

    if (!pReqParms) {
        if ((pReqParms = ReqParms_getBureauReq(pReq)))
	    return HT_OK;
        pReqParms = &DefaultReqParms;
    }
    if (!pReqParms->pCSUser) {
        if (PICS_TRACE) HTTrace("PICS: No user selected\n");
	return HT_OK;
    }
    if (CSUser_bureau(pReqParms->pCSUser)) /* handled by CSApp_netBefore */
        return HT_OK;
    url = HTAnchor_address((HTAnchor *) anchor);
    if ((translated = CSUser_acceptLabels(pReqParms->pCSUser, CSCompleteness_full))) {
        if (PICS_TRACE) HTTrace("PICS: Accept \"%s\".\n", translated);
        PUTBLOCK(translated, strlen(translated));
        HT_FREE(translated);
    }
    HT_FREE(url);
    return HT_OK;
}

/*HTParserCallback CSApp_headerParser;*/
PRIVATE int CSApp_headerParser (HTRequest * pReq, const char * token)
{
    char * label;
    char * value = 0;
    CSParse_t * pCSParse;
    ReqParms_t * pReqParms = ReqParms_getReq(pReq);

    if (!pReqParms) {
        pReqParms = &DefaultReqParms;
	DefaultReqParms.pReq = pReq;
    }
    if (!pReqParms->pCSUser) /* if there is no user */
       	return HT_OK;             /* send the document on its way */
    StrAllocCopy(value, token);
    if ((label = HTNextField(&value)) == NULL) {
        return HT_ERROR;
    }
 /*   pReqParms->disposition = CSCheckLabel_parseAndValidateLabelStr(value, pReqParms->pCSUser); */
    pCSParse = CSParse_newLabel(0, 0);

    CSParse_parseChunk(pCSParse, value, (int) strlen(value), 0);
    pReqParms->disposition = 
           CSCheckLabel_checkLabelAndUser(CSParse_getLabel(pCSParse), 
					  pReqParms->pCSUser);

    pReqParms->pCSLabel = CSParse_getLabel(pCSParse);
    CSParse_delete(pCSParse);
    if (PICS_TRACE) HTTrace("PICS: PICS mime header \"%s\" got %d.\n", value, pReqParms->disposition);
    return ReqParms_checkDisposition(pReqParms);
}

/* application functions */
PUBLIC BOOL CSApp_registerApp(CSDisposition_callback * pCallback, 
			      CSDisposition_criteria criteria, 
			      CSApp_userCallback pUserCallback, void * pVoid)
{
    HTList* conversions;

    PUserCallback = pUserCallback;

    /* set up DefaultReqParms */
    DefaultReqParms.pCallback = pCallback;
    DefaultReqParms.criteria = criteria;
    DefaultReqParms.pVoid = pVoid;

    /* Tell HTMIME which headers we will deal with */
    HTHeader_addGenerator(CSApp_headerGenerator);
    HTNetCall_addBefore(CSApp_netBefore, NULL, HT_ALL);
    HTNetCall_addAfter(CSApp_netAfter, NULL, HT_ALL);
    HTHeader_addParser(S_mimeLabel, TRUE, CSApp_headerParser);

    /* set converters so pics profiles may be read */
    conversions = HTFormat_conversion();
#if 0
    HTConversion_add(conversions, S_machRead, "*/*", CSParseMachRead, 
		     1.0, 0.0, 0.0);
    HTConversion_add(conversions, S_label, "*/*", CSParseLabel, 1.0, 0.0, 0.0);
#endif
    HTConversion_add(conversions, S_user, "*/*", CSParseUser, 1.0, 0.0, 0.0);

    HTBind_add("rat", S_machRead, "7bit", NULL, NULL, 1.0);
    HTBind_add("pcu", S_user, "7bit", NULL, NULL, 1.0);
    HTBind_add("lab", S_label, "7bit", NULL, NULL, 1.0);
    HTBind_add("url", S_URLList, "7bit", NULL, NULL, 1.0);

    if (!LoadedUsers)
        LoadedUsers = HTList_new();
    if (!ListWithBureauBefore) {
        ListWithBureauBefore = HTList_new();
	HTList_addObject(ListWithBureauBefore, (void *)CSApp_bureauBefore);
    }
    if (!ListWithBureauAfter) {
        ListWithBureauAfter = HTList_new();
	HTList_addObject(ListWithBureauAfter, (void *)CSApp_bureauAfter);
    }
    return YES;
}

PUBLIC BOOL CSApp_unregisterApp()
{
    HTHeader_deleteGenerator(CSApp_headerGenerator);
    HTHeader_deleteParser(S_mimeLabel);
    return YES;
}

PUBLIC BOOL CSApp_registerReq(HTRequest* pReq, CSUser_t * pCSUser, CSDisposition_callback pCallback, CSDisposition_criteria criteria, void * pVoid)
{
    ReqParms_t * pReqParms;
    if (!ReqParms)
      ReqParms = HTList_new();
    if ((pReqParms = ReqParms_getReq(pReq))) {
        return NO;
    }
    pReqParms = ReqParms_new(pReq, pCSUser, pCallback, criteria, pVoid);
    HTList_addObject(ReqParms, (void *)pReqParms);
    return YES;
}

PUBLIC BOOL CSApp_unregisterReq(HTRequest* pReq)
{
    ReqParms_t * pReqParms;
    if ((pReqParms = ReqParms_getReq(pReq)) == NULL) {
        return NO;
    }
    HTList_removeObject(ReqParms, (void *)pReqParms);
    ReqParms_free(pReqParms);
    return YES;
}

/* M I S C */
PUBLIC char * CSApp_libraryVersion(void)
{
    return "1.0";
}
