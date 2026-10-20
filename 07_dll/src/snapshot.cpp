#include "snapshot.h"
#include "gfx.h"

namespace ki {
namespace snap {

// Per-screen readers: where the focused item's text lives (display-object paths from the decompiled AS3).
// 'state' = the button's current timeline label: GainFocus/FocusedLoop(/RemappingLoop/Step*) when it is the live cursor.
// index mode: the screen's public getter (indexFn, default GetSelectionIndex) + the item at that index;
// scan mode: every item's state label, the focused one reported with its position among visible, labelled items.
static const char* SCREENS_JSON = R"JSON({
 "MainMenu.swf": {"scan": true, "probe": {"buildState": "currentLabel", "menuState": "mcMenuGroup.currentLabel"},
    "groups": {"main": {"item": "mcMenuGroup.mcMenuBtn{i}.mcBtn", "count": 5,
    "fields": {"label": "mcTxt.txtButton.text", "desc": "mcDescription.mcDescriptionContainer.txtDescription.text", "state": "currentLabel", "visible": "visible"}}}},
 "OptionsMenu.swf": {"groups": {
    "entries": {"item": "mcMenuGroup.mcOptionsMain.mcBtn{i}", "fields": {"label": "mcTxtLarge.txt.text", "desc": "mcTextDesc.mcTextHolder.txtDescription.text", "state": "currentLabel"}},
    "toggles": {"item": "mcMenuGroup.mcToggleMenu.mcBtn{i}", "fields": {"label": "mcTxtLarge.txt.text", "desc": "mcTextDesc.mcTextHolder.txtDescription.text",
       "desc2": "mcTextDesc.txtDescription.text", "value": "mcValueLarge.txt.text", "valueShown": "mcValueLarge.visible", "slider": "mcSliderValue.txt.text", "state": "currentLabel"}}}},
 "CharacterSelect.swf": {"custom": "charselect"},
 "PauseMenu.swf": {"scan": true, "groups": {"main": {"item": "mcMenuGroup.mcBtn{i}", "count": 7,
    "fields": {"label": "mcTxtLarge.txtButton.text", "state": "currentLabel", "visible": "visible"}}}},
 "PracticePauseMenu.swf": {"scan": true, "probe": {"category": "mcMenuGroup.mcToggle.mcFilterTxt.txtFilter.text"}, "groups": {
    "main": {"when": "mcMenuGroup.mcPauseOptionsMain.visible", "item": "mcMenuGroup.mcPauseOptionsMain.mcBtn{i}", "count": 8,
       "fields": {"label": "mcTxtLarge.txt.text", "state": "currentLabel", "visible": "visible"}},
    "toggles": {"when": "mcMenuGroup.mcToggleMenu.visible", "item": "mcMenuGroup.mcToggleMenu.mcBtn{i}", "count": 12,
       "fields": {"label": "mcTxtLarge.txt.text", "value": "mcValueLarge.txt.text", "valueShown": "mcValueLarge.visible", "slider": "mcSliderValue.txt.text",
          "desc": "mcTextDesc.mcTextHolder.txtDescription.text", "desc2": "mcTextDesc.txtDescription.text", "state": "currentLabel", "visible": "visible"}},
    "themes": {"when": "mcMenuGroup.mcThemesMenu.visible", "item": "mcMenuGroup.mcThemesMenu.mcScrollList.mcBtn{i}", "count": 8,
       "fields": {"label": "mcTxtLarge.txt.text", "state": "currentLabel", "visible": "visible"}}}},
 "Trials.swf": {"scan": true, "probe": {"difficulty": "mcMenuGroup.mcDifficulty.txtItem.text"}, "groups": {"main": {"item": "mcMenuGroup.mcBtn{i}", "count": 4,
    "fields": {"label": "mcTxtLarge.txt.text", "desc": "mcTextDesc.txtDescription.text", "state": "currentLabel", "visible": "visible"}}}},
 "Dojo.swf": {"probe": {"mode": "mcDojo.mcCategoryToggle.txtDojoMode.text", "title": "mcDojo.mcDisplayContent.mcTitle.txtDojoLevelName.text",
    "desc": "mcDojo.mcDisplayContent.mcDescription.txtDescription.text"},
    "groups": {"rows": {"item": "mcDojo.mcDojoRow{r}.mcEntry{e}", "rowSize": 8,
       "fields": {"label": "mcTxt.txtDojoLevelName.text", "state": "currentLabel", "locked": "mcLocked.visible", "done": "mcCompleted.visible"}}}},
 "Popup.swf": {"indexFn": "GetSelectedDisplayButton", "probe": {"title": "Title.text", "body": "Body.text", "hasButtons": "ButtonContainer.visible",
    "keyA": "ABUTTON.mcIcon.txt.text", "keyB": "BBUTTON.mcIcon.txt.text", "keyX": "XBUTTON.mcIcon.txt.text", "keyY": "YBUTTON.mcIcon.txt.text"},
    "groups": {"display": {"item": "DISPLAY{i}", "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel", "visible": "visible"}}}},
 "StoreMain.swf": {"scan": true, "groups": {"items": {"items": ["mcStoreMain.mcItem0", "mcStoreMain.mcItem1_1", "mcStoreMain.mcItem2_1", "mcStoreMain.mcItem3_1",
    "mcStoreMain.mcItem4_1", "mcStoreMain.mcItem5_1", "mcStoreMain.mcItem6_1", "mcStoreMain.mcItem1", "mcStoreMain.mcItem2", "mcStoreMain.mcItem3",
    "mcStoreMain.mcItem4", "mcStoreMain.mcItem5", "mcStoreMain.mcItem6", "mcStoreMain.mcItem7"],
    "fields": {"label": "mcTitle.txtItem.text", "state": "currentLabel", "visible": "visible"}}}},
 "MatchOutcome.swf": {"scan": true, "groups": {
    "p1": {"item": "mcP1RematchOptions.MenuOption_0{i}", "first": 1, "count": 4, "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel", "visible": "visible"}},
    "p2": {"item": "mcP2RematchOptions.MenuOption_0{i}", "first": 1, "count": 4, "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel", "visible": "visible"}},
    "p1stats": {"items": ["mcP1Stats.mcOffense", "mcP1Stats.mcDefense", "mcP1Stats.mcCombos", "mcP1Stats.mcVariety"], "fields": {"state": "currentLabel", "visible": "visible"}},
    "p2stats": {"items": ["mcP2Stats.mcOffense", "mcP2Stats.mcDefense", "mcP2Stats.mcCombos", "mcP2Stats.mcVariety"], "fields": {"state": "currentLabel", "visible": "visible"}}}},
 "ControllerConfig.swf": {"scan": true, "probe": {"p1start": "mcMenuGroup.mcStartPanelP1.visible", "p2start": "mcMenuGroup.mcStartPanelP2.visible",
    "p1panel": "mcMenuGroup.mcControlsPanelP1.visible", "p2panel": "mcMenuGroup.mcControlsPanelP2.visible",
    "p1prompt": "mcMenuGroup.mcStartPanelP1.mcPressStart.txtStart.text"},
    "groups": {
    "p1": {"when": "mcMenuGroup.mcControlsPanelP1.visible", "item": "mcMenuGroup.mcControlsPanelP1.mcButtonGroup.mcBtn{i}", "count": 15,
       "fields": {"state": "currentLabel", "visible": "visible", "text": "mcTxt.TxtItem.text", "value": "mcValue.TxtItem.text", "warn": "mcCmdIconGroup.mcMessageText.txt.text", "warnState": "mcCmdIconGroup.currentLabel"}},
    "p1exit": {"when": "mcMenuGroup.mcControlsPanelP1.visible", "items": ["mcMenuGroup.mcControlsPanelP1.mcButtonLeft", "mcMenuGroup.mcControlsPanelP1.mcButtonRight"],
       "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel"}},
    "p2": {"when": "mcMenuGroup.mcControlsPanelP2.visible", "item": "mcMenuGroup.mcControlsPanelP2.mcButtonGroup.mcBtn{i}", "count": 15,
       "fields": {"state": "currentLabel", "visible": "visible", "text": "mcTxt.TxtItem.text", "value": "mcValue.TxtItem.text", "warn": "mcCmdIconGroup.mcMessageText.txt.text", "warnState": "mcCmdIconGroup.currentLabel"}},
    "p2exit": {"when": "mcMenuGroup.mcControlsPanelP2.visible", "items": ["mcMenuGroup.mcControlsPanelP2.mcButtonLeft", "mcMenuGroup.mcControlsPanelP2.mcButtonRight"],
       "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel"}}}},
 "GA_HUB.swf": {"custom": "none", "probe": {"tab": "mcNavigationElements.mcTab3.mcTabTitleText.txtItem.text",
    "tabDetails": "mcNavigationElements.mcTab3.mcTabDetailsText.txtItem.text",
    "tabNotes": "mcNavigationElements.mcTab3.mcTabNotification.mcNumText.txt.text", "tabNotesShown": "mcNavigationElements.mcTab3.mcTabNotification.visible"}},
 "GA_PreLoad.swf": {"custom": "none"},
 "GA_WarRoom.swf": {"custom": "none", "probe": {"encounterShown": "mcEncountersPopup.visible"},
    "calls": {"encIdx": {"obj": "mcEncountersPopup.mcPopup.SelectionModule", "fn": "GetSelectionIndex"}}},
 "GA_SpiritLair.swf": {"scan": true, "probe": {
    "popupState": "mcPetDetailsPopup.currentLabel",
    "rechargeState": "mcAstralEnergyPopup.currentLabel",
    "rechargeWallet": "mcAstralEnergyPopup.mcAstralEnergyWallet.txtItem.text",
    "rechargeNeed": "mcAstralEnergyPopup.mcRequiresText.txtItem.text",
    "dismantleState": "mcDismantlePopup.currentLabel"},
    "calls": {"cardIdx": {"obj": "mcPanel_0", "fn": "GetSelectedCardIndex"}},
    "groups": {"decks": {"item": "mcPanel_0.mcPetDeck_{i}", "count": 10, "fields": {"state": "currentLabel", "visible": "visible"}}}},
 "GA_Archives.swf": {"scan": true, "probe": {
    "charState": "mcCharacterGroup.currentLabel",
    "charTitle": "mcCharacterGroup.mcItemTitle.txtItem.text",
    "subState": "mcCharacterSubGroup.currentLabel",
    "subTitle": "mcCharacterSubGroup.mcItemTitle.txtItem.text",
    "subDesc": "mcCharacterSubGroup.mcCharacterUnlocks.mcDescriptionText.txtItem.text",
    "invState": "mcInventoryElementsGroup.currentLabel",
    "invTitle": "mcInventoryElementsGroup.mcItemTitle.txtItem.text",
    "invDesc": "mcInventoryElementsGroup.mcCharacterUnlocks.mcDescriptionText.txtItem.text",
    "detailState": "mcPopup.currentLabel",
    "detailTitle": "mcPopup.mcTitle.txtItem.text",
    "detailBody": "mcPopup.mcBodyText.txtItem.text"},
    "groups": {"cats": {"item": "mcCategoryGroup.mcSlot{i}", "count": 4, "fields": {"state": "currentLabel", "visible": "visible"}}}},
 "GA_Barracks.swf": {"custom": "none", "probe": {
    "invState": "mcInventoryPopup.currentLabel",
    "invChar": "mcInventoryPopup.mcInventoryCharacterModule.mcCharacterName.mcCharacterNameOuter.txtItem.text",
    "invHealth": "mcInventoryPopup.mcInventoryCharacterModule.mcHealthMeter.mcHealthTitle.mcHealthTitleOuter.txtItem.text",
    "invTitle": "mcInventoryPopup.mcInventoryList.mcConsumableTitle.txtItem.text",
    "invDesc": "mcInventoryPopup.mcInventoryList.mcConsumableDescription.txtItem.text",
    "invCat": "mcInventoryPopup.mcInventoryList.mcToggle.mcCategoryName.txtItem.text",
    "invPos": "mcInventoryPopup.mcInventoryList.mcToggle.mcToggleQuantity.txtItem.text"}},
 "GA_Emporium.swf": {"custom": "none",
    "probe": {"tab0": "mcTabs.mcPacksTitle.currentLabel", "tab1": "mcTabs.mcKIGoldTitle.currentLabel", "tab2": "mcTabs.mcCraftTitle.currentLabel", "tab3": "mcTabs.mcInventoryTitle.currentLabel",
       "tab0Text": "mcTabs.mcPacksTitle.mcTitleTextBase.txtItem.text", "tab1Text": "mcTabs.mcKIGoldTitle.mcTitleTextBase.txtItem.text",
       "tab2Text": "mcTabs.mcCraftTitle.mcTitleTextBase.txtItem.text", "tab3Text": "mcTabs.mcInventoryTitle.mcTitleTextBase.txtItem.text",
       "packsShown": "mcPacksElements.visible", "goldShown": "mcKIGoldElements.visible", "craftShown": "mcCraftElements.visible", "storageShown": "mcInventoryElements.visible",
       "craftTitle": "mcCraftElements.mcItemTitle.txtItem.text", "craftPos": "mcCraftElements.mcBuyPackAmt.txtItem.text",
       "craftDesc": "mcCraftElements.mcCraftItemDetails.txtItem.text", "craftDesc2": "mcCraftElements.mcCraftItemDetails2.txtItem.text",
       "storageTitle": "mcInventoryElements.mcItemTitle.txtItem.text", "storageDesc": "mcInventoryElements.mcInventoryItemDetails.txtItem.text",
       "storageDesc2": "mcInventoryElements.mcInventoryItemDetails2.txtItem.text", "packDesc": "mcPacksElements.mcPackCellDetails.txtItem.text",
       "goldDesc": "mcKIGoldElements.mcMainCellGoldDetails.txtItem0.text", "popupShown": "mcPopupElements.visible",
       "buyState": "mcPopupElements.mcBuyPackPopup.currentLabel", "buyTitle": "mcPopupElements.mcBuyPackPopup.mcBuyPaclPopupTitle.txtItem.text",
       "buyDesc": "mcPopupElements.mcBuyPackPopup.mcBuyPopUpText.txtItem.text",
       "buyGold": "mcPopupElements.mcBuyPackPopup.mcBuyPackPopupCost.mcPackKICostText.txtItem.text",
       "buyTokens": "mcPopupElements.mcBuyPackPopup.mcBuyPackPopupCost.mcPackTokenCostText.txtItem.text",
       "revealState": "mcPopupElements.mcBuyPackPopup.mcPackPurchaseAnim.currentLabel",
       "revealTitle": "mcPopupElements.mcBuyPackPopup.mcPackPurchaseAnim.mcBuyPackTitleReveal.txtItem.text",
       "revealDesc": "mcPopupElements.mcBuyPackPopup.mcPackPurchaseAnim.mcMainCellDetails.txtItem0.text",
       "revealDesc2": "mcPopupElements.mcBuyPackPopup.mcPackPurchaseAnim.mcMainCellDetails2.txtItem0.text",
       "revealCard": "mcPopupElements.mcBuyPackPopup.mcPackPurchaseAnim.mcCell3.currentLabel",
       "goldPopState": "mcPopupElements.mcBuyKIGoldPopup.currentLabel",
       "goldPopTitle": "mcPopupElements.mcBuyKIGoldPopup.mcBuyKIGoldPopupTitle.txtItem.text",
       "goldPopDesc": "mcPopupElements.mcBuyKIGoldPopup.mcBuyKIGoldPopUpText.txtItem.text",
       "craftPopState": "mcPopupElements.mcCraftingPopup.currentLabel",
       "craftPopTitle": "mcPopupElements.mcCraftingPopup.mcCraftingPopupTitle.txtItem.text",
       "craftPopDesc": "mcPopupElements.mcCraftingPopup.mcCraftingPopupText.txtItem.text"},
    "calls": {"packIdx": {"obj": "mcPacksElements", "fn": "GetSelectedPackIndex"}, "goldIdx": {"obj": "mcKIGoldElements", "fn": "GetSelectedPackIndex"},
       "craftIdx": {"obj": "mcCraftElements", "fn": "GetSelectedPackIndex"}}},
 "GA_Loadout_Popup.swf": {"scan": true,
    "probe": {
       "confirmState": "mcWarRoom_Loadout.mcConfirmMission.currentLabel",
       "fightState": "mcWarRoom_Loadout.mcConfirmMission.mcFight.currentLabel", "deployState": "mcWarRoom_Loadout.mcConfirmMission.mcDeploy.currentLabel",
       "consState": "mcWarRoom_Loadout.mcConsumablePopup.currentLabel",
       "consTitle": "mcWarRoom_Loadout.mcConsumablePopup.mcConsumableTitle.txtItem.text",
       "consDesc": "mcWarRoom_Loadout.mcConsumablePopup.mcConsumableDescription.txtItem.text",
       "consCat": "mcWarRoom_Loadout.mcConsumablePopup.mcToggle.mcCategoryName.txtItem.text",
       "consPos": "mcWarRoom_Loadout.mcConsumablePopup.mcToggle.mcToggleQuantity.txtItem.text",
       "petState": "mcWarRoom_Loadout.mcSpiritPetPopup.currentLabel",
       "petName": "mcWarRoom_Loadout.mcSpiritPetPopup.mcPetDeck_1.mcPetDeckName.txtItem.text",
       "petDesc": "mcWarRoom_Loadout.mcSpiritPetPopup.mcPetDeck_1.mcSpiritPetInfo.txtItem.text",
       "petCat": "mcWarRoom_Loadout.mcSpiritPetPopup.mcToggle.mcCategoryName.txtItem.text",
       "petPos": "mcWarRoom_Loadout.mcSpiritPetPopup.mcToggle.mcToggleQuantity.txtItem.text",
       "rechargeState": "mcWarRoom_Loadout.mcSpiritPetPopup.mcAstralEnergyPopup.currentLabel",
       "rechargeWallet": "mcWarRoom_Loadout.mcSpiritPetPopup.mcAstralEnergyPopup.mcAstralEnergyWallet.txtItem.text"},
    "calls": {"petIndex": {"obj": "mcWarRoom_Loadout.mcSpiritPetPopup.mcPetDeck_1", "fn": "GetSelectionIndex"}},
    "groups": {
    "confirm": {"items": ["mcWarRoom_Loadout.mcConfirmMission.mcFight", "mcWarRoom_Loadout.mcConfirmMission.mcDeploy"],
       "fields": {"state": "currentLabel", "visible": "visible", "title": "mcTitle.txtItem.text", "desc": "mcDesc.txtItem.text",
          "pct": "mcPercentage.txtItem.text", "pctVisible": "mcPercentage.visible"}},
    "consumables": {"item": "mcWarRoom_Loadout.mcConsumablePopup.mcItem_{i}", "count": 10,
       "fields": {"state": "currentLabel", "visible": "visible", "qty": "mcItemQuantity.txtItem.text"}},
    "slots": {"item": "mcWarRoom_Loadout.mcLoadout.mcAddFighter_{i}", "count": 3,
       "fields": {"state": "currentLabel", "visible": "visible", "add": "mcSlot.mcAddFighter.txtItem.text", "sub": "mcFilledBacking.currentLabel",
          "consumable": "mcFilledBacking.mcConsumableTitle.txtItem.text", "pet": "mcFilledBacking.mcSpiritPetTitle.txtItem.text"}},
    "launch": {"items": ["mcWarRoom_Loadout.mcLoadout.mcLaunchMissionButton"],
       "fields": {"state": "currentLabel", "visible": "visible", "label": "mcLaunchMission.mcLaunchMissionText.txtItem.text"}},
    "fighters": {"when": "mcWarRoom_Loadout.mcCharacterPopup.visible", "item": "mcWarRoom_Loadout.mcCharacterPopup.mcCharacter_{i}", "count": 3,
       "fields": {"state": "currentLabel", "visible": "visible"}}}},
)JSON" R"JSON(
 "StoreBundles.swf": {"probe": {"hint": "mcExtraInfo.mcHintText.txtItem.text"}},
 "StoreFighters.swf": {"custom": "none", "probe": {
    "name": "mcInfo.mcFighterName.txtItem.text",
    "status": "mcInfo.mcStatus.txtItem.text"}},
 "Story.swf": {"probe": {"item": "mcStoryMain.mcUnlockDetails.txtItem.text",
    "req": "mcStoryMain.mcUnlockDetails.txtDescription.text",
    "diff": "mcStoryMain.mcUnlockDetails.txtDifficulty.text",
    "season": "mcStoryMain.mcSeasonToggle.txtSeason.text",
    "complete": "mcStoryMain.mcComplete.txtComplete.text"}},
 "CommandList.swf": {"indexFn": "GetSelectionIndex"},
 "FightArchiveMain.swf": {"indexFn": "GetSelectionIndex", "probe": {"buildState": "currentLabel", "transitioning": "transitioning"}},
 "Stats.swf": {"custom": "none", "probe": {"mainState": "mcMainStats.currentLabel", "allState": "mcAllStats.currentLabel",
    "category": "mcAllStats.mcToggle.txtCategory.text", "subcategory": "mcAllStats.mcToggle.txtSubCategory.text"}},
 "FightTitles.swf": {"indexFn": "GetSelectionIndex", "probe": {"mainState": "mcMenuGroup.mcTitlesMain.currentLabel",
    "detailState": "mcMenuGroup.mcTitlesDetail.currentLabel"}},
 "Leaderboards.swf": {"indexFn": "GetSelectionIndex", "probe": {"filter": "mcFilterLabel.txtItem.text",
    "category": "mcMenuGroup.mcToggle.txtCategory.text", "subcategory": "mcMenuGroup.mcToggle.txtSubCategory.text",
    "loading": "mcLoadingContainer.visible"}},
 "ShadowLeaderboards.swf": {"scan": true, "probe": {"filter": "mcToggle.mcFilterText.txtItem.text",
    "category": "mcToggle.mcCategoryCenter.txtItem.text", "loading": "mcLoadingContainer.visible",
    "hdrWins": "mcListItems.mcColumnHeaders.mcWinsTitle.txtItem.text", "hdrLosses": "mcListItems.mcColumnHeaders.mcLossesTitle.txtItem.text",
    "hdrRank": "mcListItems.mcColumnHeaders.mcRankTitle.txtItem.text"},
    "calls": {"rowIdx": {"obj": "mcListItems", "fn": "GetCurrentIndex"}},
    "groups": {"rows": {"item": "mcListItems.mcListItem{i}", "count": 5, "fields": {"state": "currentLabel", "visible": "visible",
       "rank": "RankPosition.txtItem.text", "tag": "GamerTag.txtItem.text", "charname": "CharacterName.txtItem.text",
       "wins": "WinsValue.mcText.txtItem.text", "losses": "LossesValue.mcText.txtItem.text", "ranking": "RankingValue.mcText.txtItem.text"}}}},
 "Replays.swf": {"indexFn": "GetSelectionIndex"},
 "StageSelect.swf": {"scan": true, "probe": {"musicState": "mcMusicSelect.currentLabel"}, "groups": {
    "music": {"item": "mcMusicSelect.mcTrackText{i}", "count": 15, "fields": {"label": "mcText.txtItem.text", "state": "currentLabel", "visible": "visible"}}}},
 "GameSettingsPopUp.swf": {"scan": true, "groups": {"settings": {"item": "mcGSPopUpText.mcBtn{i}", "count": 3,
    "fields": {"label": "mcTxt.TxtItem.text", "value": "mcValue.TxtItem.text", "state": "currentLabel", "visible": "visible"}}}},
 "MultiplayerLobby.swf": {"scan": true, "probe": {"screen": "mcLobby.currentLabel",
    "lobTitle": "mcLobby.mcLobbyInside.txtLobbyTitle.text", "lobHost": "mcLobby.mcLobbyInside.txtGamerTag.text",
    "lobSlots": "mcLobby.mcLobbyInside.txtSlotsPublicPrivate.text", "lobRank": "mcLobby.mcLobbyInside.txtButton.text"}, "groups": {
    "main": {"item": "mcLobby.mcLobbyMain.mcBtn{i}", "count": 2,
       "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel", "visible": "visible"}},
    "create": {"item": "mcLobby.mcLobbyCreate.mcToggle{i}", "count": 8,
       "fields": {"label": "mcTxt.txt.text", "value": "mcValue.txt.text", "state": "currentLabel", "visible": "visible"}},
    "search": {"item": "mcLobby.mcLobbySearching.mcToggle{i}", "count": 5,
       "fields": {"label": "mcTxt.txt.text", "value": "mcValue.txt.text", "state": "currentLabel", "visible": "visible"}},
    "lobby": {"item": "mcLobby.mcLobbyInside.mcGamerTag{i}", "count": 8,
       "fields": {"label": "mcTxt.txtButton.text", "ratio": "mcWinsLosses.txtButton.text", "crown": "mcCrown.visible",
          "state": "currentLabel", "visible": "visible"}}}},
 "MultiplayerExhibition.swf": {"indexFn": "GetSelectionIndex", "probe": {
    "s0": "mcExhibitionMain.mcExhibition.mcBtn0.currentLabel", "s1": "mcExhibitionMain.mcExhibition.mcToggle1.currentLabel",
    "s2": "mcExhibitionMain.mcExhibition.mcToggle2.currentLabel", "s3": "mcExhibitionMain.mcExhibition.mcToggle3.currentLabel",
    "s4": "mcExhibitionMain.mcExhibition.mcToggle4.currentLabel", "s5": "mcExhibitionMain.mcExhibition.mcToggle5.currentLabel",
    "s6": "mcExhibitionMain.mcExhibition.mcBtn2.currentLabel",
    "v1": "mcExhibitionMain.mcExhibition.mcToggle1.mcValue.txt.text", "v2": "mcExhibitionMain.mcExhibition.mcToggle2.mcValue.txt.text",
    "v3": "mcExhibitionMain.mcExhibition.mcToggle3.mcValue.txt.text", "v4": "mcExhibitionMain.mcExhibition.mcToggle4.mcValue.txt.text",
    "v5": "mcExhibitionMain.mcExhibition.mcToggle5.mcValue.txt.text"}},
 "MultiplayerLadder.swf": {"scan": true,
    "calls": {"lbIdx": {"obj": "mcLeaderboards", "fn": "GetSelectionIndex"}},
    "probe": {"tutState": "mcTutotialScreen.currentLabel",
       "cardRank": "mcPlayerCardP1.mcProLadderElements.mcProLadderTextAmt.txtItem.text",
       "lbState": "mcLeaderboards.currentLabel", "lbLoading": "mcLeaderboards.mcLoadingContainer.visible",
       "lbCat": "mcLeaderboards.mcMenuGroup.mcToggle.txtCategory.text",
       "lbFilter": "mcLeaderboards.mcMenuGroup.mcFilterLabel.txtItem.text",
       "wT": "mcTutotialScreen.mcWelcomeTitle.txtItem.text", "wB": "mcTutotialScreen.mcWelcomeBody.txtItem.text",
       "qT": "mcTutotialScreen.mcTimeToQualifyTitle.txtItem.text", "qB": "mcTutotialScreen.mcTimeToQualifyBody.txtItem.text",
       "oT": "mcTutotialScreen.mcOnTrackTitle.txtItem.text", "oB": "mcTutotialScreen.mcOnTrackBody.txtItem.text",
       "r1T": "mcTutotialScreen.mcRules01Title.txtItem.text", "r1B": "mcTutotialScreen.mcRules01Body.txtItem.text",
       "r2T": "mcTutotialScreen.mcRules02Title.txtItem.text", "r2B": "mcTutotialScreen.mcRules02Body.txtItem.text",
       "r3T": "mcTutotialScreen.mcRules03Title.txtItem.text", "r3B": "mcTutotialScreen.mcRules03Body.txtItem.text",
       "kcT": "mcTutotialScreen.mcKillerCongratsTitle.txtItem.text", "kcB": "mcTutotialScreen.mcKillerCongratsBody.txtItem.text",
       "krT": "mcTutotialScreen.mcKillerRulesTitle.txtItem.text", "krB": "mcTutotialScreen.mcKillerRulesBody.txtItem.text",
       "rsT": "mcTutotialScreen.mcResetTitle.txtItem.text", "rsB": "mcTutotialScreen.mcResetBody.txtItem.text",
       "psT": "mcTutotialScreen.mcProStatusTitle.txtItem.text", "psB": "mcTutotialScreen.mcProStatusBody.txtItem.text",
       "pwT": "mcTutotialScreen.mcProbationWarningTitle.txtItem.text", "pwB": "mcTutotialScreen.mcProbationWarningBody.txtItem.text",
       "ipT": "mcTutotialScreen.mcInProbationTitle.txtItem.text", "ipB": "mcTutotialScreen.mcInProbationBody.txtItem.text",
       "mrT": "mcTutotialScreen.mcMonthlyResetTitle.txtItem.text", "mrB": "mcTutotialScreen.mcMonthlyResetBody.txtItem.text",
       "idT": "mcTutotialScreen.mcInProbationDetailsTitle.txtItem.text", "idB": "mcTutotialScreen.mcInProbationDetailsBody.txtItem.text"},
    "groups": {
    "menu": {"when": "mcPlayerCardP1.visible",
       "items": ["mcMenuGroup.mcMenuGroup.mcMenuBtn0.mcBtn0", "mcMenuGroup.mcMenuGroup.mcMenuBtn1.mcBtn0",
                 "mcMenuGroup.mcMenuGroup.mcMenuBtn3.mcBtn0", "mcMenuGroup.mcMenuGroup.mcMenuBtn4.mcBtn0"],
       "fields": {"label": "mcTxt.txtButton.text", "desc": "mcDescription.mcDescriptionContainer.txtDescription.text",
          "state": "currentLabel", "visible": "visible"}}}}
})JSON";

static json SCREENS;
static std::string s_lastGood;
static bool s_inSnapshot = false;

void init() { SCREENS = json::parse(SCREENS_JSON); }
bool hasScreen(const std::string& swf) { return SCREENS.contains(swf); }
std::string lastGoodSwf() { return s_lastGood; }

static bool isFocusedLabel(const json& s) {
    if (!s.is_string()) return false;
    const std::string& v = s.get_ref<const std::string&>();
    if (v == "RemappingLoop" || startsWith(v, "Step")) return true;   // a toggle only steps while it has the cursor
    return v.find("Focus") != std::string::npos && !startsWith(v, "Lose") && !startsWith(v, "Unfocused");
}

static std::vector<std::string> itemPaths(const json& gc) {
    std::vector<std::string> out;
    const json& items = jget(gc, "items");
    if (items.is_array()) { for (const auto& p : items) out.push_back(p.get<std::string>()); return out; }
    int first = jint(jget(gc, "first")), count = jint(jget(gc, "count"));
    std::string tmpl = jstr(gc, "item");
    for (int i = first; i < first + count; i++) out.push_back(replaceAll(tmpl, "{i}", std::to_string(i)));
    return out;
}

static json readFields(const gfx::GValue& ref, const std::string& item, const json& fields, const json& skip) {
    json f = json::object();
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (skip.is_object() && skip.contains(it.key())) { f[it.key()] = skip[it.key()]; continue; }
        json v = gfx::readPath(ref, item + "." + it.value().get<std::string>());
        if (!v.is_discarded()) f[it.key()] = v;
    }
    return f;
}

static void snapshotCharSelect(const gfx::GValue& ref, json& rec) {
    json cs = json::object();
    gfx::GValue fs;
    if (gfx::memberObj(ref, "mcFighterSelect", fs)) {
        json a = gfx::invoke(fs, "GetFocusedFighterIndex", {0}), b = gfx::invoke(fs, "GetFocusedFighterIndex", {1});
        if (!a.is_discarded()) cs["f0"] = a;
        if (!b.is_discarded()) cs["f1"] = b;
        gfx::release(fs);
    }
    for (int side = 1; side <= 2; side++) {
        gfx::GValue co, cl;
        std::string s = std::to_string(side - 1);
        if (gfx::memberObj(ref, gfx::cstr("mcP" + std::to_string(side) + "CostumeSelect"), co)) {
            json c = gfx::invoke(co, "GetSelectedCostumeIndex", {}), slot = gfx::invoke(co, "GetSelectedCustomSlot", {});
            if (!c.is_discarded()) cs["c" + s] = c;
            if (!slot.is_discarded()) cs["slot" + s] = slot;
            gfx::release(co);
        }
        if (gfx::memberObj(ref, gfx::cstr("mcP" + std::to_string(side) + "ColorSelect"), cl)) {
            json c = gfx::invoke(cl, "GetSelectedColorBackendIndex", {});
            if (!c.is_discarded()) cs["col" + s] = c;
            gfx::release(cl);
        }
    }
    rec["cs"] = cs;
}

static void snapshotScan(const gfx::GValue& ref, const json& cfg, json& rec) {
    rec["index"] = -1;
    json groups = json::object();
    const json& gs = jget(cfg, "groups");
    for (auto it = gs.begin(); it != gs.end(); ++it) {
        const json& gc = it.value();
        std::string when = jstr(gc, "when");
        if (!when.empty()) {
            json w = gfx::readPath(ref, when);
            if (w.is_boolean() && !w.get<bool>()) { groups[it.key()] = json{{"count", 0}, {"hidden", true}}; continue; }   // whole menu hidden
        }
        const json& fields = jget(gc, "fields");
        std::string stateField = jstr(fields, "state"), visField = jstr(fields, "visible"), labelField = jstr(fields, "label");
        std::vector<std::string> paths = itemPaths(gc);
        json focused;
        int pos = -1, visible = 0, first = jint(jget(gc, "first"));
        for (size_t i = 0; i < paths.size(); i++) {
            json st = gfx::readPath(ref, paths[i] + "." + stateField);
            bool vis = true;
            if (!visField.empty()) { json v = gfx::readPath(ref, paths[i] + "." + visField); vis = !(v.is_boolean() && !v.get<bool>()); }
            if (vis && !labelField.empty()) {   // an unused button can stay visible with empty text: do not count it
                json lb = gfx::readPath(ref, paths[i] + "." + labelField);
                if (lb.is_discarded() || (lb.is_string() && (lb.get<std::string>().empty() || lb.get<std::string>() == "missing text"))) vis = false;
                else if (isFocusedLabel(st) && focused.is_null()) focused = readFields(ref, paths[i], fields, json{{"state", st}, {"visible", true}, {"label", lb}});
            } else if (vis && isFocusedLabel(st) && focused.is_null()) focused = readFields(ref, paths[i], fields, json{{"state", st}, {"visible", true}});
            if (vis) visible++;
            if (!focused.is_null() && pos < 0) { focused["index"] = first + (int)i; pos = visible - 1; }
        }
        json g = focused.is_null() ? json::object() : focused;
        if (!focused.is_null()) g["pos"] = pos;
        g["count"] = visible;
        groups[it.key()] = g;
    }
    rec["groups"] = groups;
}

json snapshot(const std::string& swfIn, const std::string& why) {
    json rec = json::object();
    if (!gfx::g_root || s_inSnapshot) return rec;
    s_inSnapshot = true;
    std::string swf = swfIn;
    int64_t t0 = nowMs();
    gfx::GValue ref;
    bool ok = gfx::getRef(swf, ref);
    if (!ok && !s_lastGood.empty() && s_lastGood != swf) {
        // a shared component called a script of a movie that is not loaded (e.g. the practice menu's toggles call
        // OptionsMenu.SetVariables): the cursor is still on the last screen we could read
        if (gfx::getRef(s_lastGood, ref)) { swf = s_lastGood; ok = true; }
    }
    if (!ok) {
        s_inSnapshot = false;
        rec = json{{"ev", "focus"}, {"swf", swf}, {"why", why}, {"missing", true}};
        return rec;
    }
    s_lastGood = swf;
    rec = json{{"ev", "focus"}, {"swf", swf}, {"why", why}, {"t", t0}};
    const json& cfg = jget(SCREENS, swf.c_str());
    const json& probe = jget(cfg, "probe");
    if (probe.is_object()) {
        json p = json::object();
        for (auto it = probe.begin(); it != probe.end(); ++it) { json v = gfx::readPath(ref, it.value().get<std::string>()); if (!v.is_discarded()) p[it.key()] = v; }
        rec["probe"] = p;
    }
    const json& calls = jget(cfg, "calls");   // getters on nested objects: {"name": {"obj": "a.b", "fn": "GetSelectionIndex"}}
    if (calls.is_object()) {
        json p = rec.contains("probe") ? rec["probe"] : json::object();
        for (auto it = calls.begin(); it != calls.end(); ++it) {
            json v = gfx::invokePath(ref, jstr(it.value(), "obj"), jstr(it.value(), "fn").c_str());
            if (!v.is_discarded()) p[it.key()] = v;
        }
        rec["probe"] = p;
    }
    std::string custom = jstr(cfg, "custom");
    if (custom == "charselect") { snapshotCharSelect(ref, rec); rec["index"] = -1; }
    else if (custom == "none") rec["index"] = -1;
    else if (jtruthy(jget(cfg, "scan"))) snapshotScan(ref, cfg, rec);
    else {
        std::string indexFn = jstr(cfg, "indexFn");
        json idx = gfx::invoke(ref, indexFn.empty() ? "GetSelectionIndex" : indexFn.c_str(), {});
        if (idx.is_number()) rec["index"] = (int)idx.get<double>(); else rec["index"] = nullptr;
        const json& gs = jget(cfg, "groups");
        if (gs.is_object() && rec["index"].is_number()) {
            int i = rec["index"].get<int>();
            std::string modePath = jstr(cfg, "mode");
            if (!modePath.empty()) { json m = gfx::readPath(ref, modePath); if (m.is_boolean()) rec["mode"] = m; }
            json groups = json::object();
            for (auto it = gs.begin(); it != gs.end(); ++it) {
                const json& gc = it.value();
                std::string item = jstr(gc, "item");
                int rowSize = jint(jget(gc, "rowSize"));
                if (rowSize) item = replaceAll(replaceAll(item, "{r}", std::to_string(i / rowSize)), "{e}", std::to_string(i % rowSize));
                else item = replaceAll(item, "{i}", std::to_string(i));
                groups[it.key()] = readFields(ref, item, jget(gc, "fields"), json());
            }
            rec["groups"] = groups;
        }
    }
    gfx::release(ref);
    rec["ms"] = nowMs() - t0;
    s_inSnapshot = false;
    return rec;
}

}  // namespace snap
}  // namespace ki
