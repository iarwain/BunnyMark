/**
 * @file bunnymark.cpp
 * @date 30-Jun-2021
 */

#include "orx.h"

#ifdef __orxMSVC__

/* Requesting high performance dedicated GPU on hybrid laptops */
__declspec(dllexport) unsigned long NvOptimusEnablement         = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance  = 1;

#endif // __orxMSVC__

static orxOBJECT       *pstCurrentBunny                     = orxNULL;
static const orxS32     s32MaxBunnyCount                    = 1000000;
static volatile orxS32  s32ActiveBunnyCount                 = 0;
static orxFLOAT         fGravity                            = orxFLOAT_0;
static orxVECTOR        vScreenSize                         = {};
static orxVECTOR        avBunnyPositions[s32MaxBunnyCount]  = {};
static orxVECTOR        avBunnySpeeds[s32MaxBunnyCount]     = {};

orxSTATUS orxFASTCALL UpdateBunnies(void *_pContext)
{
    orxCLOCK_INFO  *pstClockInfo  = (orxCLOCK_INFO *)_pContext;
    orxSTATUS       eResult       = orxSTATUS_SUCCESS;

    // For all active bunnies
    for(orxS32 i = 0, iCount = s32ActiveBunnyCount; i < iCount; i++)
    {
        // Updates its speed
        avBunnySpeeds[i].fY += fGravity * pstClockInfo->fDT;

        // Moves it
        avBunnyPositions[i].fX += avBunnySpeeds[i].fX * pstClockInfo->fDT;
        avBunnyPositions[i].fY += avBunnySpeeds[i].fY * pstClockInfo->fDT;

        // Constrains it
        if(avBunnyPositions[i].fX < orxFLOAT_0)
        {
            avBunnySpeeds[i].fX  = -avBunnySpeeds[i].fX;
            avBunnyPositions[i].fX    = orxFLOAT_0;
        }
        else if(avBunnyPositions[i].fX > vScreenSize.fX)
        {
            avBunnySpeeds[i].fX  = -avBunnySpeeds[i].fX;
            avBunnyPositions[i].fX    = vScreenSize.fX;
        }
        if(avBunnyPositions[i].fY < orxFLOAT_0)
        {
            avBunnySpeeds[i].fY  = -avBunnySpeeds[i].fY;
            avBunnyPositions[i].fY    = orxFLOAT_0;
        }
        else if (avBunnyPositions[i].fY > vScreenSize.fY)
        {
            avBunnySpeeds[i].fY  = -avBunnySpeeds[i].fY;
            avBunnyPositions[i].fY    = vScreenSize.fY;
        }
    }

  // Done!
  return eResult;
}

orxSTATUS orxFASTCALL EventHandler(const orxEVENT *_pstEvent)
{
    static orxS32   ss32BunnyIndex = 0;
    orxSTATUS       eResult = orxSTATUS_SUCCESS;

    if(_pstEvent->eID == orxRENDER_EVENT_START)
    {
        // Resets internal display index
        ss32BunnyIndex = 0;

        // Stores FPS
        orxConfig_SetS32("FPS", orxFPS_GetFPS());

        // Runs update task
        orxThread_RunTask(UpdateBunnies, orxNULL, orxNULL, _pstEvent->pContext);
    }
    else
    {
        orxOBJECT *pstObject;

        // Gets sending object
        pstObject = orxOBJECT(_pstEvent->hSender);

        // Bunny?
        if(!orxString_Compare(orxObject_GetName(pstObject), "Bunny"))
        {
            orxDISPLAY_TRANSFORM    stTransform;
            orxCOLOR                stColor;
            orxVECTOR               vScale;
            orxBITMAP              *pstBitmap;
            orxRGBA                 stRGBA = orx2RGBA(255, 255, 255, 255);
            orxCHAR                 acBuffer[32] = {};

            // Gets its current bitmap
            pstBitmap = orxTexture_GetBitmap(orxObject_GetWorkingTexture(pstObject));

            // Gets its scale
            orxObject_GetScale(pstObject, &vScale);

            // Gets its color
            if(orxObject_HasColor(pstObject))
            {
                stRGBA = orxColor_ToRGBA(orxObject_GetColor(pstObject, &stColor));
            }

            // Inits transform
            stTransform.fRepeatX    =
            stTransform.fRepeatY    = orxFLOAT_1;
            stTransform.fScaleX     = vScale.fX;
            stTransform.fScaleY     = vScale.fY;
            stTransform.fSrcX       =
            stTransform.fSrcY       =
            stTransform.fRotation   = orxFLOAT_0;

            // For all active bunnies
            orxString_NPrint(acBuffer, sizeof(acBuffer), "%016llX", orxStructure_GetGUID(pstObject));
            orxConfig_PushSection(acBuffer);
            for(orxS32 s32Count = orxMIN(s32ActiveBunnyCount, orxConfig_GetS32("Count") + ss32BunnyIndex);
                ss32BunnyIndex < s32Count;
                ss32BunnyIndex++)
            {
                // Updates transform
                stTransform.fDstX   = avBunnyPositions[ss32BunnyIndex].fX;
                stTransform.fDstY   = avBunnyPositions[ss32BunnyIndex].fY;

                // Renders it
                orxDisplay_TransformBitmap(pstBitmap, &stTransform, stRGBA, orxDISPLAY_SMOOTHING_OFF, orxDISPLAY_BLEND_MODE_ALPHA);
            }
            orxConfig_PopSection();

            // Don't render it
            eResult = orxSTATUS_FAILURE;
        }
    }

    // Done!
    return eResult;
}

/** Update function, it has been registered to be called every tick of the core clock
 */
void orxFASTCALL Update(const orxCLOCK_INFO *_pstClockInfo, void *_pContext)
{
    orxS32    s32Delta  = 0;
    orxSTATUS eResult   = orxSTATUS_SUCCESS;

    // Add bunnies?
    if(orxInput_IsActive("+Bunny"))
    {
        orxS32  s32Delta = orxConfig_GetS32("Delta");
        orxCHAR acBuffer[32] = {};

        // New bunny batch?
        if(orxInput_HasNewStatus("+Bunny"))
        {
            orxU64 PreviousBunnyGUID = pstCurrentBunny ? orxStructure_GetGUID(pstCurrentBunny) : 0;

            // Creates new random batch
            pstCurrentBunny = orxObject_CreateFromConfig("Bunny");
            orxString_NPrint(acBuffer, sizeof(acBuffer), "%016llX", orxStructure_GetGUID(pstCurrentBunny));

            // Keep track of the previous one (for removal)
            orxConfig_PushSection(acBuffer);
            orxConfig_SetU64("Previous", PreviousBunnyGUID);
            orxConfig_PopSection();
        }

        // Updates global bunny count
        s32Delta = orxMIN(s32Delta, s32MaxBunnyCount - s32ActiveBunnyCount);
        s32ActiveBunnyCount += s32Delta;
        orxConfig_SetS32("Count", s32ActiveBunnyCount);

        // Inits all bunnies
        for(orxS32 i = s32ActiveBunnyCount - s32Delta; i < s32ActiveBunnyCount; i++)
        {
            orxConfig_GetVector("InitPos", &avBunnyPositions[i]);
            orxConfig_GetVector("InitSpeed", &avBunnySpeeds[i]);
        }

        // Updates local bunny count
        orxString_NPrint(acBuffer, sizeof(acBuffer), "%016llX", orxStructure_GetGUID(pstCurrentBunny));
        orxConfig_PushSection(acBuffer);
        orxConfig_SetS32("Count", orxConfig_GetS32("Count") + s32Delta);
        orxConfig_PopSection();
    }

    // Remove bunnies?
    if(orxInput_HasBeenActivated("-Bunny"))
    {
        // Has bunny batch?
        if(pstCurrentBunny)
        {
            orxCHAR acBuffer[32];
            orxS32  s32Delta;

            // Gets batch count, delete current batch & retrieve former bunny batch
            orxString_NPrint(acBuffer, sizeof(acBuffer), "%016llX", orxStructure_GetGUID(pstCurrentBunny));
            orxConfig_PushSection(acBuffer);
            s32Delta = orxConfig_GetS32("Count");
            orxObject_Delete(pstCurrentBunny);
            pstCurrentBunny = orxOBJECT(orxStructure_Get(orxConfig_GetU64("Previous")));
            orxConfig_PopSection();

            // Updates global bunny count
            s32ActiveBunnyCount -= s32Delta;
            orxConfig_SetS32("Count", s32ActiveBunnyCount);
        }
    }

    // Screenshot?
    if(orxInput_IsActive("Screenshot") && orxInput_HasNewStatus("Screenshot"))
    {
        // Captures it
        orxScreenshot_Capture();
    }

    // Should quit?
    if(orxInput_IsActive("Quit"))
    {
        // Send close event
        orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_CLOSE);
    }
}

/** Init function, it is called when all orx's modules have been initialized
 */
orxSTATUS orxFASTCALL Init()
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

    // Create the viewport
    orxViewport_CreateFromConfig("MainViewport");

    // Create the scene
    orxObject_CreateFromConfig("Scene");

    // Pushes bunny config section
    orxConfig_PushSection("Bunny");

    // Adds render event handler
    orxEvent_AddHandlerWithContext(orxEVENT_TYPE_RENDER, EventHandler, (void *)orxClock_GetInfo(orxClock_Get(orxCLOCK_KZ_CORE)));
    orxEvent_SetHandlerIDFlags(EventHandler, orxEVENT_TYPE_RENDER, orxNULL, orxEVENT_GET_FLAG(orxRENDER_EVENT_START) | orxEVENT_GET_FLAG(orxRENDER_EVENT_OBJECT_START), orxEVENT_KU32_MASK_ID_ALL);

    // Gets gravity
    fGravity = orxConfig_GetFloat("Gravity");

    // Gets screen size
    orxConfig_PushSection("Display");
    vScreenSize.fX = orxConfig_GetFloat("ScreenWidth");
    vScreenSize.fY = orxConfig_GetFloat("ScreenHeight");
    orxConfig_PopSection();

    // Inits all bunnies
    for(orxS32 i = 0; i < s32MaxBunnyCount; i++)
    {
        orxConfig_GetVector("InitPos", &avBunnyPositions[i]);
        orxConfig_GetVector("InitSpeed", &avBunnySpeeds[i]);
    }

    // Register the Update function to the core clock
    orxClock_Register(orxClock_Get(orxCLOCK_KZ_CORE), Update, orxNULL, orxMODULE_ID_MAIN, orxCLOCK_PRIORITY_NORMAL);

    // Done!
    return orxSTATUS_SUCCESS;
}

/** Run function, it should not contain any game logic
 */
orxSTATUS orxFASTCALL Run()
{
    // Return orxSTATUS_FAILURE to instruct orx to quit
    return orxSTATUS_SUCCESS;
}

/** Exit function, it is called before exiting from orx
 */
void orxFASTCALL Exit()
{
    // Let Orx clean all our mess automatically. :)
}

/** Bootstrap function, it is called before config is initialized, allowing for early resource storage definitions
 */
orxSTATUS orxFASTCALL Bootstrap()
{
    // Add config storage to find the initial config file
    orxResource_AddStorage(orxCONFIG_KZ_RESOURCE_GROUP, "../data/config", orxFALSE);

    // Return orxSTATUS_FAILURE to prevent orx from loading the default config file
    return orxSTATUS_SUCCESS;
}

/** Main function
 */
int main(int argc, char **argv)
{
    // Set the bootstrap function to provide at least one resource storage before loading any config files
    orxConfig_SetBootstrap(Bootstrap);

    // Execute our game
    orx_Execute(argc, argv, Init, Run, Exit);

    // Done!
    return EXIT_SUCCESS;
}
