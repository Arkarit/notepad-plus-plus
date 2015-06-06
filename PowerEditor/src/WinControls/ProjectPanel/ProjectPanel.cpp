// This file is part of Notepad++ project
// Copyright (C)2003 Don HO <don.h@free.fr>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// Note that the GPL places important restrictions on "derived works", yet
// it does not provide a detailed definition of that term.  To avoid      
// misunderstandings, we consider an application to constitute a          
// "derivative work" for the purpose of this license if it does any of the
// following:                                                             
// 1. Integrates source code from Notepad++.
// 2. Integrates/includes/aggregates Notepad++ into a proprietary executable
//    installer, such as those produced by InstallShield.
// 3. Links to a library or executes a program that does any of the above.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "ProjectPanel.h"
#include "resource.h"
#include "tinyxml.h"
#include "FileDialog.h"
#include "localization.h"
#include "Parameters.h"

#define CX_BITMAP         16
#define CY_BITMAP         16

#define INDEX_CLEAN_ROOT      0
#define INDEX_DIRTY_ROOT      1
#define INDEX_PROJECT         2
#define INDEX_OPEN_NODE	      3
#define INDEX_CLOSED_NODE     4
#define INDEX_LEAF            5
#define INDEX_LEAF_INVALID    6
#define INDEX_OPEN_MONITOR    7
#define INDEX_CLOSED_MONITOR  8
#define INDEX_INVALID_MONITOR 9
#define INDEX_LEAF_MONITOR    10

#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))

#pragma warning( push )
#pragma warning( disable : 4100 )

INT_PTR CALLBACK ProjectPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG :
        {

			ProjectPanel::initMenus();

			// Create toolbar menu
			int style = WS_CHILD | WS_VISIBLE | CCS_ADJUSTABLE | TBSTYLE_AUTOSIZE | TBSTYLE_FLAT | TBSTYLE_LIST;
			_hToolbarMenu = CreateWindowEx(0,TOOLBARCLASSNAME,NULL, style,
								   0,0,0,0,_hSelf,(HMENU)0, _hInst, NULL);
			TBBUTTON tbButtons[2];

			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance())->getNativeLangSpeaker();
			generic_string workspace_entry = pNativeSpeaker->getProjectPanelLangMenuStr("Entries", 0, PM_WORKSPACEMENUENTRY);
			generic_string edit_entry = pNativeSpeaker->getProjectPanelLangMenuStr("Entries", 1, PM_EDITMENUENTRY);

			tbButtons[0].idCommand = IDB_PROJECT_BTN;
			tbButtons[0].iBitmap = I_IMAGENONE;
			tbButtons[0].fsState = TBSTATE_ENABLED;
			tbButtons[0].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
			tbButtons[0].iString = (INT_PTR)workspace_entry.c_str();

			tbButtons[1].idCommand = IDB_EDIT_BTN;
			tbButtons[1].iBitmap = I_IMAGENONE;
			tbButtons[1].fsState = TBSTATE_ENABLED;
			tbButtons[1].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
			tbButtons[1].iString = (INT_PTR)edit_entry.c_str();

			SendMessage(_hToolbarMenu, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
			SendMessage(_hToolbarMenu, TB_ADDBUTTONS,       (WPARAM)sizeof(tbButtons) / sizeof(TBBUTTON),       (LPARAM)&tbButtons);
			SendMessage(_hToolbarMenu, TB_AUTOSIZE, 0, 0); 
			ShowWindow(_hToolbarMenu, SW_SHOW);

			_treeView.init(_hInst, _hSelf, ID_PROJECTTREEVIEW);
			_directoryWatcher = new DirectoryWatcher(_treeView.getHSelf());
			_directoryWatcher->startThread();

			setImageList(IDI_PROJECT_WORKSPACE, IDI_PROJECT_WORKSPACEDIRTY, IDI_PROJECT_PROJECT, IDI_PROJECT_FOLDEROPEN, IDI_PROJECT_FOLDERCLOSE, IDI_PROJECT_FILE, IDI_PROJECT_FILEINVALID, IDI_PROJECT_FOLDERMONITOROPEN, IDI_PROJECT_FOLDERMONITORCLOSE, IDI_PROJECT_FOLDERMONITORINVALID, IDI_PROJECT_FILEMONITOR);
			_treeView.addCanNotDropInList(INDEX_LEAF);
			_treeView.addCanNotDropInList(INDEX_LEAF_INVALID);
			_treeView.addCanNotDropInList(INDEX_OPEN_MONITOR);
			_treeView.addCanNotDropInList(INDEX_CLOSED_MONITOR);
			_treeView.addCanNotDropInList(INDEX_INVALID_MONITOR);
			_treeView.addCanNotDropInList(INDEX_LEAF_MONITOR);

			_treeView.addCanNotDragOutList(INDEX_CLEAN_ROOT);
			_treeView.addCanNotDragOutList(INDEX_DIRTY_ROOT);
			_treeView.addCanNotDragOutList(INDEX_PROJECT);
			_treeView.addCanNotDragOutList(INDEX_OPEN_MONITOR);
			_treeView.addCanNotDragOutList(INDEX_CLOSED_MONITOR);
			_treeView.addCanNotDragOutList(INDEX_INVALID_MONITOR);
			_treeView.addCanNotDragOutList(INDEX_LEAF_MONITOR);

			_treeView.display();
			if (!openWorkSpace(_workSpaceFilePath.c_str()))
				newWorkSpace();

            return TRUE;
        }

		
		case WM_MOUSEMOVE:
			if (_treeView.isDragging())
				_treeView.dragItem(_hSelf, LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_LBUTTONUP:
			if (_treeView.isDragging())
				if (_treeView.dropItem())
					setWorkSpaceDirty(true);
			break;

		case WM_NOTIFY:
		{
			notified((LPNMHDR)lParam);
		}
		return TRUE;

        case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            RECT toolbarMenuRect;
            ::GetClientRect(_hToolbarMenu, &toolbarMenuRect);

            ::MoveWindow(_hToolbarMenu, 0, 0, width, toolbarMenuRect.bottom, TRUE);

			HWND hwnd = _treeView.getHSelf();
			if (hwnd)
				::MoveWindow(hwnd, 0, toolbarMenuRect.bottom + 2, width, height - toolbarMenuRect.bottom - 2, TRUE);
            break;
        }

        case WM_CONTEXTMENU:
			if (!_treeView.isDragging())
				showContextMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return TRUE;

		case WM_COMMAND:
		{
			popupMenuCmd(LOWORD(wParam));
			break;
		}

		case WM_DESTROY:
        {
			_treeView.destroy();
			destroyMenus();
			::DestroyWindow(_hToolbarMenu);
            break;
        }
		case WM_KEYDOWN:
			//if (wParam == VK_F2)
			{
				::MessageBoxA(NULL,"vkF2","",MB_OK);
			}
			break;

        default :
            return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
    }
	return DockingDlgInterface::run_dlgProc(message, wParam, lParam);
}

static HTREEITEM treeviewInsertFunc(TreeView* treeView, HTREEITEM hParent, const TreeViewData* parentTVData, const TCHAR *toInsertName, const TreeViewData* toInsertTVData)
{
//return TVI_LAST;


	const ProjectPanelFileData* parentData = (const ProjectPanelFileData*) parentTVData;
	const ProjectPanelFileData* toInsertData = (const ProjectPanelFileData*) toInsertTVData;

	if( !parentData->isFolderMonitor() && !parentData->isFolderMonitorRoot() )
		return TVI_LAST;

	HTREEITEM hLastItem = NULL;
    for (HTREEITEM hItem = treeView->getChildFrom(hParent);
        hItem != NULL;
        hLastItem = hItem, hItem = treeView->getNextSibling(hItem))
    {
		TVITEM tvItem;
		tvItem.mask = TVIF_PARAM;
		tvItem.hItem = hItem;
		SendMessage(treeView->getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
		ProjectPanelFileData* itemData = (ProjectPanelFileData*) tvItem.lParam;

		// folders always before files
		if( itemData->isFolderMonitor() && !toInsertData->isFolderMonitor() )
			continue;

		if( !itemData->isFolderMonitor() && toInsertData->isFolderMonitor()
		||  lstrcmpi(itemData->_name.c_str(),toInsertName) > 0)
			return hLastItem == NULL ? TVI_FIRST : hLastItem;

    }

	return TVI_LAST;

}

void ProjectPanel::checkIfNeedSave(const TCHAR *title)
{
	if (_isDirty)
	{
		display();
		int res = ::MessageBox(_hSelf, TEXT("The workspace was modified. Do you want to save it?"), title, MB_YESNO | MB_ICONQUESTION);
		if (res == IDYES)
		{
			if (!saveWorkSpace())
				::MessageBox(_hSelf, TEXT("Your workspace was not saved."), title, MB_OK | MB_ICONERROR);
		}
		//else if (res == IDNO)
			// Don't save so do nothing here
	}
}

void ProjectPanel::initMenus()
{
	_hWorkSpaceMenu = ::CreatePopupMenu();
	
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance())->getNativeLangSpeaker();

	generic_string new_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_NEWWS, PM_NEWWORKSPACE);
	generic_string open_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_OPENWS, PM_OPENWORKSPACE);
	generic_string reload_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_RELOADWS, PM_RELOADWORKSPACE);
	generic_string save_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_SAVEWS, PM_SAVEWORKSPACE);
	generic_string saveas_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_SAVEASWS, PM_SAVEASWORKSPACE);
	generic_string saveacopyas_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_SAVEACOPYASWS, PM_SAVEACOPYASWORKSPACE);
	generic_string newproject_workspace = pNativeSpeaker->getProjectPanelLangMenuStr("WorkspaceMenu", IDM_PROJECT_NEWPROJECT, PM_NEWPROJECTWORKSPACE);

	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, IDM_PROJECT_NEWWS, new_workspace.c_str());
	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, IDM_PROJECT_OPENWS, open_workspace.c_str());
	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, IDM_PROJECT_RELOADWS, reload_workspace.c_str());
	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, IDM_PROJECT_SAVEWS, save_workspace.c_str());
	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, IDM_PROJECT_SAVEASWS, saveas_workspace.c_str());
	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, IDM_PROJECT_SAVEACOPYASWS, saveacopyas_workspace.c_str());
	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, (UINT)-1, 0);
	::InsertMenu(_hWorkSpaceMenu, 0, MF_BYCOMMAND, IDM_PROJECT_NEWPROJECT, newproject_workspace.c_str());

	generic_string edit_moveup = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_MOVEUP, PM_MOVEUPENTRY);
	generic_string edit_movedown = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_MOVEDOWN, PM_MOVEDOWNENTRY);
	generic_string edit_rename = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_RENAME, PM_EDITRENAME);
	generic_string edit_addfolder = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_NEWFOLDER, PM_EDITNEWFOLDER);
	generic_string edit_addfiles = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_ADDFILES, PM_EDITADDFILES);
	generic_string edit_addfilesRecursive = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_ADDFILESRECUSIVELY, PM_EDITADDFILESRECUSIVELY);
	generic_string edit_addfolderMonitor = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_EDITADDFOLDERMONITOR, PM_EDITADDFOLDERMONITOR);
	generic_string edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_DELETEFOLDER, PM_EDITREMOVE);

	_hProjectMenu = ::CreatePopupMenu();
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEUP, edit_moveup.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEDOWN, edit_movedown.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, UINT(-1), 0);
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_RENAME, edit_rename.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_NEWFOLDER, edit_addfolder.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILES, edit_addfiles.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILESRECUSIVELY, edit_addfilesRecursive.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_EDITADDFOLDERMONITOR, edit_addfolderMonitor.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_DELETEFOLDER, edit_remove.c_str());

	edit_moveup = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_MOVEUP, PM_MOVEUPENTRY);
	edit_movedown = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_MOVEDOWN, PM_MOVEDOWNENTRY);
	edit_rename = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_RENAME, PM_EDITRENAME);
	edit_addfolder = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_NEWFOLDER, PM_EDITNEWFOLDER);
	edit_addfiles = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_ADDFILES, PM_EDITADDFILES);
	edit_addfilesRecursive = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_ADDFILESRECUSIVELY, PM_EDITADDFILESRECUSIVELY);
	edit_addfolderMonitor = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_EDITADDFOLDERMONITOR, PM_EDITADDFOLDERMONITOR);
	edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_DELETEFOLDER, PM_EDITREMOVE);

	_hFolderMenu = ::CreatePopupMenu();
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEUP,        edit_moveup.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEDOWN,      edit_movedown.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, UINT(-1), 0);
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_RENAME,        edit_rename.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_NEWFOLDER,     edit_addfolder.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILES,      edit_addfiles.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILESRECUSIVELY, edit_addfilesRecursive.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_EDITADDFOLDERMONITOR, edit_addfolderMonitor.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_DELETEFOLDER, edit_remove.c_str());

	edit_moveup = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_MOVEUP, PM_MOVEUPENTRY);
	edit_movedown = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_MOVEDOWN, PM_MOVEDOWNENTRY);
	edit_rename = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_RENAME, PM_EDITRENAME);
	edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_DELETEFILE, PM_EDITREMOVE);
	generic_string edit_modifyfile = pNativeSpeaker->getProjectPanelLangMenuStr("FileMenu", IDM_PROJECT_MODIFYFILEPATH, PM_EDITMODIFYFILE);

	_hFileMenu = ::CreatePopupMenu();
	::InsertMenu(_hFileMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEUP, edit_moveup.c_str());
	::InsertMenu(_hFileMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEDOWN, edit_movedown.c_str());
	::InsertMenu(_hFileMenu, 0, MF_BYCOMMAND, UINT(-1), 0);
	::InsertMenu(_hFileMenu, 0, MF_BYCOMMAND, IDM_PROJECT_RENAME, edit_rename.c_str());
	::InsertMenu(_hFileMenu, 0, MF_BYCOMMAND, IDM_PROJECT_DELETEFILE, edit_remove.c_str());
	::InsertMenu(_hFileMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MODIFYFILEPATH, edit_modifyfile.c_str());

	_hFolderMonitorMenu = ::CreatePopupMenu();
	::InsertMenu(_hFolderMonitorMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEUP, edit_moveup.c_str());
	::InsertMenu(_hFolderMonitorMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEDOWN, edit_movedown.c_str());
	::InsertMenu(_hFolderMonitorMenu, 0, MF_BYCOMMAND, IDM_PROJECT_DELETEFILE, edit_remove.c_str());

}


BOOL ProjectPanel::setImageList(int root_clean_id, int root_dirty_id, int project_id, int open_node_id, int closed_node_id, int leaf_id, int ivalid_leaf_id, int open_monitor_id, int closed_monitor_id, int invalid_monitor_id, int file_monitor_id) 
{
	HBITMAP hbmp;
	COLORREF maskColour = RGB(192, 192, 192);
	const int nbBitmaps = 7;

	// Creation of image list
	if ((_hImaLst = ImageList_Create(CX_BITMAP, CY_BITMAP, ILC_COLOR32 | ILC_MASK, nbBitmaps, 0)) == NULL) 
		return FALSE;

	// Add the bmp in the list
	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(root_clean_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(root_dirty_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(project_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(open_node_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(closed_node_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(leaf_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(ivalid_leaf_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(open_monitor_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(closed_monitor_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(invalid_monitor_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(file_monitor_id));
	if (hbmp == NULL)
		return FALSE;
	ImageList_AddMasked(_hImaLst, hbmp, maskColour);
	DeleteObject(hbmp);

	if (ImageList_GetImageCount(_hImaLst) < nbBitmaps)
		return FALSE;

	// Set image list to the tree view
	TreeView_SetImageList(_treeView.getHSelf(), _hImaLst, TVSIL_NORMAL);

	return TRUE;
}


void ProjectPanel::destroyMenus() 
{
	::DestroyMenu(_hWorkSpaceMenu);
	::DestroyMenu(_hProjectMenu);
	::DestroyMenu(_hFolderMenu);
	::DestroyMenu(_hFileMenu);
	::DestroyMenu(_hFolderMonitorMenu);
}

bool ProjectPanel::openWorkSpace(const TCHAR *projectFileName)
{
	TiXmlDocument *pXmlDocProject = new TiXmlDocument(projectFileName);
	bool loadOkay = pXmlDocProject->LoadFile();
	if (!loadOkay)
		return false;

	TiXmlNode *root = pXmlDocProject->FirstChild(TEXT("NotepadPlus"));
	if (!root) 
		return false;


	TiXmlNode *childNode = root->FirstChildElement(TEXT("Project"));
	if (!childNode) 
		return false;

	if (!::PathFileExists(projectFileName))
		return false;

	_treeView.removeAllItems();
	_workSpaceFilePath = projectFileName;

	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance())->getNativeLangSpeaker();
	generic_string workspace = pNativeSpeaker->getAttrNameStr(PM_WORKSPACEROOTNAME, "ProjectManager", "WorkspaceRootName");
	HTREEITEM rootItem = _treeView.addItem(workspace.c_str(), TVI_ROOT, INDEX_CLEAN_ROOT, new ProjectPanelFileData(_directoryWatcher, workspace.c_str(), NULL, nodeType_root));

	for ( ; childNode ; childNode = childNode->NextSibling(TEXT("Project")))
	{
		const TCHAR* name = (childNode->ToElement())->Attribute(TEXT("name"));
		HTREEITEM projectItem = _treeView.addItem(name, rootItem, INDEX_PROJECT, new ProjectPanelFileData(_directoryWatcher, name, NULL, nodeType_project));
		buildTreeFrom(childNode, projectItem);
	}
	setWorkSpaceDirty(false);
	_treeView.expand(rootItem);

	delete pXmlDocProject;
	return loadOkay;
}

void ProjectPanel::newWorkSpace()
{
	NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance())->getNativeLangSpeaker();
	generic_string workspace = pNativeSpeaker->getAttrNameStr(PM_WORKSPACEROOTNAME, "ProjectManager", "WorkspaceRootName");
	_treeView.addItem(workspace.c_str(), TVI_ROOT, INDEX_CLEAN_ROOT, new ProjectPanelFileData(_directoryWatcher, workspace.c_str(), NULL, nodeType_root));
	setWorkSpaceDirty(false);
	_workSpaceFilePath = TEXT("");
}

bool ProjectPanel::saveWorkSpace()
{
	if (_workSpaceFilePath == TEXT(""))
	{
		return saveWorkSpaceAs(false);
	}
	else
	{
		writeWorkSpace();
		setWorkSpaceDirty(false);
		_isDirty = false;
		return true;
	} 
}

bool ProjectPanel::writeWorkSpace(TCHAR *projectFileName)
{
    //write <NotepadPlus>: use the default file name if new file name is not given
	const TCHAR * fn2write = projectFileName?projectFileName:_workSpaceFilePath.c_str();
	TiXmlDocument projDoc(fn2write);
    TiXmlNode *root = projDoc.InsertEndChild(TiXmlElement(TEXT("NotepadPlus")));

	TCHAR textBuffer[MAX_PATH];
	TVITEM tvItem;
    tvItem.mask = TVIF_TEXT;
    tvItem.pszText = textBuffer;
    tvItem.cchTextMax = MAX_PATH;

    //for each project, write <Project>
    HTREEITEM tvRoot = _treeView.getRoot();
    if (!tvRoot)
      return false;

    for (HTREEITEM tvProj = _treeView.getChildFrom(tvRoot);
        tvProj != NULL;
        tvProj = _treeView.getNextSibling(tvProj))
    {        
        tvItem.hItem = tvProj;
        SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
        //printStr(tvItem.pszText);

		TiXmlNode *projRoot = root->InsertEndChild(TiXmlElement(TEXT("Project")));
		projRoot->ToElement()->SetAttribute(TEXT("name"), tvItem.pszText);
		buildProjectXml(projRoot, tvProj, fn2write);
    }
    projDoc.SaveFile();
	return true;
}

void ProjectPanel::buildProjectXml(TiXmlNode *node, HTREEITEM hItem, const TCHAR* fn2write)
{
	TCHAR textBuffer[MAX_PATH];
	TVITEM tvItem;
	tvItem.mask = TVIF_TEXT | TVIF_PARAM;
	tvItem.pszText = textBuffer;
	tvItem.cchTextMax = MAX_PATH;

    for (HTREEITEM hItemNode = _treeView.getChildFrom(hItem);
		hItemNode != NULL;
		hItemNode = _treeView.getNextSibling(hItemNode))
	{
		tvItem.hItem = hItemNode;
		SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
		ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*) tvItem.lParam;

		if (tvFileInfo.isFile())
		{
			generic_string newFn = getRelativePath(tvFileInfo._filePath, fn2write);
			TiXmlNode *fileLeaf = node->InsertEndChild(TiXmlElement(TEXT("File")));
			fileLeaf->ToElement()->SetAttribute(TEXT("name"), newFn.c_str());
		}
		else if (tvFileInfo.isFolderMonitorRoot())
		{
			generic_string newFn = getRelativePath(tvFileInfo._filePath, fn2write);
			TiXmlNode *fileLeaf = node->InsertEndChild(TiXmlElement(TEXT("FolderMonitor")));
			fileLeaf->ToElement()->SetAttribute(TEXT("name"), newFn.c_str());
		}
		else
		{
			TiXmlNode *folderNode = node->InsertEndChild(TiXmlElement(TEXT("Folder")));
			folderNode->ToElement()->SetAttribute(TEXT("name"), tvItem.pszText);
			buildProjectXml(folderNode, hItemNode, fn2write);
		}
	}
}

generic_string ProjectPanel::getRelativePath(const generic_string & filePath, const TCHAR *workSpaceFileName)
{
	TCHAR wsfn[MAX_PATH];
	lstrcpy(wsfn, workSpaceFileName);
	::PathRemoveFileSpec(wsfn);

	size_t pos_found = filePath.find(wsfn);
	if (pos_found == generic_string::npos)
		return filePath;
	const TCHAR *relativeFile = filePath.c_str() + lstrlen(wsfn);
	if (relativeFile[0] == '\\')
		++relativeFile;
	return relativeFile;
}

bool ProjectPanel::buildTreeFrom(TiXmlNode *projectRoot, HTREEITEM hParentItem)
{
	for (TiXmlNode *childNode = projectRoot->FirstChildElement();
		childNode ;
		childNode = childNode->NextSibling())
	{
		const TCHAR *v = childNode->Value();
		if (lstrcmp(TEXT("Folder"), v) == 0)
		{
			const TCHAR* name =(childNode->ToElement())->Attribute(TEXT("name"));
			HTREEITEM addedItem = _treeView.addItem(name, hParentItem, INDEX_CLOSED_NODE, new ProjectPanelFileData(_directoryWatcher, name, NULL, nodeType_folder));
			if (!childNode->NoChildren())
			{
				bool isOK = buildTreeFrom(childNode, addedItem);
				if (!isOK)
					return false;
			}
		}
		if (lstrcmp(TEXT("FolderMonitor"), v) == 0)
		{

			const TCHAR *strValue = (childNode->ToElement())->Attribute(TEXT("name"));
			generic_string fullPath = getAbsoluteFilePath(strValue);

			generic_string newFolderLabel(fullPath);
			const size_t lastSlashIdx = newFolderLabel.find_last_of(TEXT("\\/"));
			if (std::string::npos != lastSlashIdx)
			{
				newFolderLabel.erase(0, lastSlashIdx + 1);
				if( newFolderLabel.empty() ) // drive
				{
					newFolderLabel = fullPath;
					newFolderLabel.erase(lastSlashIdx);
				}
			}
			addFolder(hParentItem, newFolderLabel.c_str(), true, true, fullPath.c_str());
		}
		else if (lstrcmp(TEXT("File"), v) == 0)
		{
			const TCHAR *strValue = (childNode->ToElement())->Attribute(TEXT("name"));
			generic_string fullPath = getAbsoluteFilePath(strValue);
			TCHAR *strValueLabel = ::PathFindFileName(strValue);
			int iImage = ::PathFileExists(fullPath.c_str())?INDEX_LEAF:INDEX_LEAF_INVALID;
			_treeView.addItem(strValueLabel, hParentItem, iImage, new ProjectPanelFileData(_directoryWatcher, strValueLabel, fullPath.c_str(), nodeType_file));
		}
	}
	return true;
}

void ProjectPanel::rebuildFolderMonitorTree(HTREEITEM hParentItem, const ProjectPanelFileData& tvFileInfo)
{
	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hParentItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, (LPARAM)&tvItem);

	int iImg;
	if (tvFileInfo._nodeType == nodeType_monitorFolderRoot)
	{
		if (!::PathFileExists(tvFileInfo._filePath.c_str()))
		{
			iImg = INDEX_INVALID_MONITOR;
			_treeView.removeAllChildren(hParentItem);
		}
		else
		{
			iImg = (tvItem.state & TVIS_EXPANDED) ? INDEX_OPEN_MONITOR : INDEX_CLOSED_MONITOR;
		}
	}
	else if (tvFileInfo._nodeType == nodeType_monitorFolder)
	{
		if (!::PathFileExists(tvFileInfo._filePath.c_str()))
		{
			_treeView.removeItem(hParentItem);
			return;
		}
		else
		{
			iImg = (tvItem.state & TVIS_EXPANDED) ? INDEX_OPEN_MONITOR : INDEX_CLOSED_MONITOR;
		}
	}
	else
	{
		assert(0);
		return;
	}

	removeDummies(hParentItem);
	ProjectPanelDirectory currDir( this, hParentItem );
	Directory newDir(tvFileInfo._filePath);
	currDir.synchronizeTo(newDir);

	_treeView.setItemImage(hParentItem, iImg, iImg);

}

generic_string ProjectPanel::getAbsoluteFilePath(const TCHAR * relativePath)
{
	if (!::PathIsRelative(relativePath))
		return relativePath;

	TCHAR absolutePath[MAX_PATH];
	lstrcpy(absolutePath, _workSpaceFilePath.c_str());
	::PathRemoveFileSpec(absolutePath);
	::PathAppend(absolutePath, relativePath);
	return absolutePath;
}

void ProjectPanel::openSelectFile()
{
	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = _treeView.getSelection();
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);

	NodeType nType = getNodeType(tvItem.hItem);
	ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*)tvItem.lParam;

	if ((nType == nodeType_file || nType == nodeType_monitorFile) && (tvFileInfo.isFile() || tvFileInfo.isFileMonitor()))
	{
		tvItem.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		if (::PathFileExists(tvFileInfo._filePath.c_str()))
		{
			::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)(tvFileInfo._filePath.c_str()));
			tvItem.iImage = tvFileInfo.isFileMonitor() ? INDEX_LEAF_MONITOR : INDEX_LEAF;
			tvItem.iSelectedImage = tvFileInfo.isFileMonitor() ? INDEX_LEAF_MONITOR : INDEX_LEAF;
		}
		else
		{
			tvItem.iImage = INDEX_LEAF_INVALID;
			tvItem.iSelectedImage = INDEX_LEAF_INVALID;
		}
		TreeView_SetItem(_treeView.getHSelf(), &tvItem);
	}
}


HMENU ProjectPanel::getContextMenu(HTREEITEM hTreeItem) const
{
	HMENU hMenu;
	NodeType nodeType = getNodeType(hTreeItem);
	if (nodeType == nodeType_root)
		hMenu = _hWorkSpaceMenu;
	else if (nodeType == nodeType_project)
		hMenu = _hProjectMenu;
	else if (nodeType == nodeType_folder)
		hMenu = _hFolderMenu;
	else if (nodeType == nodeType_monitorFolderRoot)
		hMenu = _hFolderMonitorMenu;
	else if (nodeType == nodeType_monitorFolder || nodeType == nodeType_monitorFile)
		hMenu = NULL; // currently no context menu for monitored files
	else //nodeType_file
		hMenu = _hFileMenu;
	return hMenu;

}

void ProjectPanel::onTreeItemAdded(bool afterClone, HTREEITEM hItem, TreeViewData* newData)
{
	ProjectPanelFileData* tvInfo = getInfo(newData);
	tvInfo->setItem( hItem );
	if (tvInfo->isFolderMonitorRoot())
		tvInfo->watchDir(true);
}

void ProjectPanel::onMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case DIRECTORYWATCHER_UPDATE:
		{
			HTREEITEM hItem = (HTREEITEM)lParam;
			if (_treeView.itemValid(hItem))
			{

				TVITEM tvItem;
				tvItem.hItem = hItem;
				tvItem.mask = TVIF_PARAM;
				SendMessage(hwnd, TVM_GETITEM, 0,(LPARAM)&tvItem);
				treeItemChanged(hItem, (TreeViewData*) tvItem.lParam);
			}
		}
		break;
	}

}

void ProjectPanel::treeItemChanged(HTREEITEM hTreeItem, TreeViewData* data)
{

	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hTreeItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, (LPARAM)&tvItem);

	ProjectPanelFileData& tvFileInfo = *getInfo(data);
	int iImg;
	switch (tvFileInfo._nodeType)
	{
		case nodeType_root:
			iImg = _isDirty ? INDEX_DIRTY_ROOT : INDEX_CLEAN_ROOT;
			break;
		case nodeType_project:
			iImg = INDEX_PROJECT;
			break;
		case nodeType_folder:
			iImg = (tvItem.state & TVIS_EXPANDED) ? INDEX_OPEN_NODE : INDEX_CLOSED_NODE;
			break;
		case nodeType_file:
			iImg = ::PathFileExists(tvFileInfo._filePath.c_str()) ? INDEX_LEAF : INDEX_LEAF_INVALID;
			break;
		case nodeType_monitorFile:
			iImg = INDEX_LEAF_MONITOR;
			break;
		case nodeType_monitorFolderRoot:
		case nodeType_monitorFolder:
			rebuildFolderMonitorTree(hTreeItem, tvFileInfo);
			return;
		default:
			return;
	}
	_treeView.setItemImage(hTreeItem, iImg, iImg);

}

void ProjectPanel::expandOrCollapseMonitorFolder(bool expand, HTREEITEM hItem)
{

	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, (LPARAM)&tvItem);
	ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*)(tvItem.lParam);
	if (!(tvFileInfo.isFolderMonitor() || tvFileInfo.isFolderMonitorRoot()) )
		return;

	if (expand)
	{
		removeDummies(hItem);
//		_treeView.removeAllChildren(hItem);
		if (tvFileInfo.isFolderMonitor())
			tvFileInfo.watchDir(true);
	}
	else
	{
		if (tvFileInfo.isFolderMonitor())
			tvFileInfo.watchDir(false);
//		_treeView.removeAllChildren(hItem);
//		_treeView.addItem( TEXT(""), hItem, INDEX_LEAF_MONITOR, new ProjectPanelFileData(_directoryWatcher, TEXT(""), TEXT(""), nodeType_dummy ));
	}

	_directoryWatcher->update();

}

void ProjectPanel::removeDummies(HTREEITEM hTreeItem)
{
	HTREEITEM hItemNode = _treeView.getChildFrom(hTreeItem);
	if (hItemNode == NULL)
		return;

	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hItemNode;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, (LPARAM)&tvItem);
	ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*)(tvItem.lParam);

	if (tvFileInfo.isDummy())
		_treeView.removeAllChildren(hTreeItem);

}

void ProjectPanel::notified(LPNMHDR notification)
{
	if ((notification->hwndFrom == _treeView.getHSelf()))
	{
		TCHAR textBuffer[MAX_PATH];
		TVITEM tvItem;
		tvItem.mask = TVIF_TEXT | TVIF_PARAM;
		tvItem.pszText = textBuffer;
		tvItem.cchTextMax = MAX_PATH;

		switch (notification->code)
		{
			case NM_DBLCLK:
			{
				openSelectFile();
			}
			break;
	
			case TVN_ENDLABELEDIT:
			{
				LPNMTVDISPINFO tvnotif = (LPNMTVDISPINFO)notification;
				if (!tvnotif->item.pszText)
					return;
				if (getNodeType(tvnotif->item.hItem) == nodeType_root)
					return;

				ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*)tvnotif->item.lParam;

				// Processing for only File case
				if (tvFileInfo.isFile()) 
				{
					// Get the old label
					tvItem.hItem = _treeView.getSelection();
					::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
					size_t len = lstrlen(tvItem.pszText);

					// Find the position of old label in File path
					generic_string &filePath = tvFileInfo._filePath;
					size_t found = filePath.rfind(tvItem.pszText);

					// If found the old label, replace it with the modified one
					if (found != generic_string::npos)
						filePath.replace(found, len, tvnotif->item.pszText);

					// Check the validity of modified file path
					tvItem.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
					if (::PathFileExists(filePath.c_str()))
					{
						tvItem.iImage = tvFileInfo.isFileMonitor() ? INDEX_LEAF_MONITOR : INDEX_LEAF;
						tvItem.iSelectedImage = tvFileInfo.isFileMonitor() ? INDEX_LEAF_MONITOR : INDEX_LEAF;
					}
					else
					{
						tvItem.iImage = INDEX_LEAF_INVALID;
						tvItem.iSelectedImage = INDEX_LEAF_INVALID;
					}
					TreeView_SetItem(_treeView.getHSelf(), &tvItem);
				}

				// For File, Folder and Project
				::SendMessage(_treeView.getHSelf(), TVM_SETITEM, 0,(LPARAM)(&(tvnotif->item)));
				setWorkSpaceDirty(true);
			}
			break;

			case TVN_GETINFOTIP:
			{
				LPNMTVGETINFOTIP lpGetInfoTip = (LPNMTVGETINFOTIP)notification;
				generic_string *str = NULL ;

				if (_treeView.getRoot() == lpGetInfoTip->hItem)
				{
					str = &_workSpaceFilePath;
				}
				else
				{
					str = &((ProjectPanelFileData *)lpGetInfoTip->lParam)->_filePath;
					if (str->empty())
						return;
				}
				lpGetInfoTip->pszText = (LPTSTR)str->c_str();
				lpGetInfoTip->cchTextMax = str->size();
			}
			break;

			case TVN_KEYDOWN:
			{
				//tvItem.hItem = _treeView.getSelection();
				//::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
				LPNMTVKEYDOWN ptvkd = (LPNMTVKEYDOWN)notification;
				
				if (ptvkd->wVKey == VK_DELETE)
				{
					HTREEITEM hItem = _treeView.getSelection();
					NodeType nType = getNodeType(hItem);
					if (nType == nodeType_project || nType == nodeType_folder || nType == nodeType_monitorFolderRoot)
						popupMenuCmd(IDM_PROJECT_DELETEFOLDER);
					else if (nType == nodeType_file)
						popupMenuCmd(IDM_PROJECT_DELETEFILE);
				}
				else if (ptvkd->wVKey == VK_RETURN)
				{
					HTREEITEM hItem = _treeView.getSelection();
					NodeType nType = getNodeType(hItem);
					if (nType == nodeType_file)
						openSelectFile();
					else
						_treeView.toggleExpandCollapse(hItem);
				}
				else if (ptvkd->wVKey == VK_UP)
				{
					if (0x80 & GetKeyState(VK_CONTROL))
					{
						popupMenuCmd(IDM_PROJECT_MOVEUP);
					}
				}
				else if (ptvkd->wVKey == VK_DOWN)
				{
					if (0x80 & GetKeyState(VK_CONTROL))
					{
						popupMenuCmd(IDM_PROJECT_MOVEDOWN);
					}
				}
				else if (ptvkd->wVKey == VK_F2)
					popupMenuCmd(IDM_PROJECT_RENAME);
				
			}
			break;

			case TVN_ITEMEXPANDED:
			{
				LPNMTREEVIEW nmtv = (LPNMTREEVIEW)notification;
				tvItem.hItem = nmtv->itemNew.hItem;
				tvItem.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;

				if (getNodeType(nmtv->itemNew.hItem) == nodeType_folder)
				{
					if (nmtv->action == TVE_COLLAPSE)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, INDEX_CLOSED_NODE, INDEX_CLOSED_NODE);
					}
					else if (nmtv->action == TVE_EXPAND)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, INDEX_OPEN_NODE, INDEX_OPEN_NODE);
					}
				}
				else if (getNodeType(nmtv->itemNew.hItem) == nodeType_monitorFolder || getNodeType(nmtv->itemNew.hItem) == nodeType_monitorFolderRoot)
				{
					if (nmtv->action == TVE_COLLAPSE)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, INDEX_CLOSED_MONITOR, INDEX_CLOSED_MONITOR);
					}
					else if (nmtv->action == TVE_EXPAND)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, INDEX_OPEN_MONITOR, INDEX_OPEN_MONITOR);
					}
				}

				expandOrCollapseMonitorFolder(nmtv->action == TVE_EXPAND, nmtv->itemNew.hItem);

			}
			break;

			case TVN_BEGINDRAG:
			{
				//printStr(TEXT("hello"));
				_treeView.beginDrag((LPNMTREEVIEW)notification);
				
			}
			break;
		}
	}
}

void ProjectPanel::setWorkSpaceDirty(bool isDirty)
{
	_isDirty = isDirty;
	int iImg = _isDirty?INDEX_DIRTY_ROOT:INDEX_CLEAN_ROOT;
	_treeView.setItemImage(_treeView.getRoot(), iImg, iImg);
}

NodeType ProjectPanel::getNodeType(HTREEITEM hItem) const
{
	TVITEM tvItem;
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_PARAM;
	SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
	ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*)tvItem.lParam;
	return tvFileInfo._nodeType;
}

void ProjectPanel::showContextMenu(int x, int y)
{
	TVHITTESTINFO tvHitInfo;
	HTREEITEM hTreeItem;

	// Detect if the given position is on the element TVITEM
	tvHitInfo.pt.x = x;
	tvHitInfo.pt.y = y;
	tvHitInfo.flags = 0;
	ScreenToClient(_treeView.getHSelf(), &(tvHitInfo.pt));
	hTreeItem = TreeView_HitTest(_treeView.getHSelf(), &tvHitInfo);

	if (tvHitInfo.hItem != NULL)
	{
		// Make item selected
		_treeView.selectItem(tvHitInfo.hItem);

		// get clicked item type
		HMENU hMenu = getContextMenu(tvHitInfo.hItem);
		if (hMenu != NULL)
			TrackPopupMenu(hMenu, TPM_LEFTALIGN, x, y, 0, _hSelf, NULL);
	}
}

POINT ProjectPanel::getMenuDisplyPoint(int iButton)
{
	POINT p;
	RECT btnRect;
	SendMessage(_hToolbarMenu, TB_GETITEMRECT, iButton, (LPARAM)&btnRect);

	p.x = btnRect.left;
	p.y = btnRect.top + btnRect.bottom;
	ClientToScreen(_hToolbarMenu, &p);
	return p;
}

HTREEITEM ProjectPanel::addFolder(HTREEITEM hTreeItem, const TCHAR *folderName, bool virtl, bool root, const TCHAR *monitorPath, bool sortIn)
{
	NodeType nodeType(nodeType_folder);
	int iconindex = INDEX_CLOSED_NODE;

	if (virtl)
	{
		if (root)
		{
			nodeType = nodeType_monitorFolderRoot;
			iconindex = ::PathFileExists(monitorPath) ? INDEX_CLOSED_MONITOR : INDEX_INVALID_MONITOR;
		}
		else
		{
			nodeType = nodeType_monitorFolder;
			iconindex = INDEX_CLOSED_MONITOR;
		}
	}


	HTREEITEM addedItem = _treeView.addItem(folderName, hTreeItem, iconindex, new ProjectPanelFileData(_directoryWatcher, folderName, monitorPath, nodeType), sortIn ? treeviewInsertFunc : NULL);

	if (virtl)
		_treeView.addItem( TEXT(""), addedItem, INDEX_LEAF_MONITOR, new ProjectPanelFileData(_directoryWatcher,TEXT(""), TEXT(""), nodeType_dummy ));

	if (getNodeType(hTreeItem) != nodeType_monitorFolderRoot && getNodeType(hTreeItem) != nodeType_monitorFolder)
	{
		TreeView_Expand(_treeView.getHSelf(), hTreeItem, TVE_EXPAND);
		TreeView_EditLabel(_treeView.getHSelf(), addedItem);
	}

	if (getNodeType(hTreeItem) == nodeType_folder)
		_treeView.setItemImage(hTreeItem, INDEX_OPEN_NODE, INDEX_OPEN_NODE);
	else if (getNodeType(hTreeItem) == nodeType_monitorFolderRoot)
		_treeView.setItemImage(hTreeItem, INDEX_OPEN_MONITOR, INDEX_OPEN_MONITOR);

	return addedItem;
}

void ProjectPanel::popupMenuCmd(int cmdID)
{
	// get selected item handle
	HTREEITEM hTreeItem = _treeView.getSelection();
	if (!hTreeItem)
		return;

	switch (cmdID)
	{
		//
		// Toolbar menu buttons
		//
		case IDB_PROJECT_BTN:
		{
		  POINT p = getMenuDisplyPoint(0);
		  TrackPopupMenu(_hWorkSpaceMenu, TPM_LEFTALIGN, p.x, p.y, 0, _hSelf, NULL);
		}
		break;

		case IDB_EDIT_BTN:
		{
			POINT p = getMenuDisplyPoint(1);
			HMENU hMenu = getContextMenu(hTreeItem);
			if (hMenu)
				TrackPopupMenu(hMenu, TPM_LEFTALIGN, p.x, p.y, 0, _hSelf, NULL);
		}
		break;

		//
		// Toolbar menu commands
		//
		case IDM_PROJECT_NEWPROJECT :
		{
			HTREEITEM root = _treeView.getRoot();

			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance())->getNativeLangSpeaker();
			generic_string newProjectLabel = pNativeSpeaker->getAttrNameStr(PM_NEWPROJECTNAME, "ProjectManager", "NewProjectName");
			HTREEITEM addedItem = _treeView.addItem(newProjectLabel.c_str(),  root, INDEX_PROJECT, new ProjectPanelFileData(_directoryWatcher, newProjectLabel.c_str(), NULL, nodeType_project));
			setWorkSpaceDirty(true);
			_treeView.expand(hTreeItem);
			TreeView_EditLabel(_treeView.getHSelf(), addedItem);
		}
		break;

		case IDM_PROJECT_NEWWS :
		{
			if (_isDirty)
			{
				int res = ::MessageBox(_hSelf, TEXT("The current workspace was modified. Do you want to save the current project?"), TEXT("New Workspace"), MB_YESNOCANCEL | MB_ICONQUESTION | MB_APPLMODAL);
				if (res == IDYES)
				{
					if (!saveWorkSpace())
						return;
				}
				else if (res == IDNO)
				{
					// Don't save so do nothing here
				}
				else if (res == IDCANCEL) 
				{
					// User cancels action "New Workspace" so we interrupt here
					return;
				}
			}
			_treeView.removeAllItems();
			newWorkSpace();
		}
		break;

		case IDM_PROJECT_RENAME :
			TreeView_EditLabel(_treeView.getHSelf(), hTreeItem);
		break;

		case IDM_PROJECT_NEWFOLDER :
		{
			NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance())->getNativeLangSpeaker();
			generic_string newFolderLabel = pNativeSpeaker->getAttrNameStr(PM_NEWFOLDERNAME, "ProjectManager", "NewFolderName");
			addFolder(hTreeItem, newFolderLabel.c_str());
			setWorkSpaceDirty(true);
		}
		break;

		case IDM_PROJECT_MOVEDOWN :
		{
			if (_treeView.moveDown(hTreeItem))
				setWorkSpaceDirty(true);
		}
		break;

		case IDM_PROJECT_MOVEUP :
		{
			if (_treeView.moveUp(hTreeItem))
				setWorkSpaceDirty(true);
		}
		break;

		case IDM_PROJECT_ADDFILES :
		{
			addFiles(hTreeItem);
			if (getNodeType(hTreeItem) == nodeType_folder)
				_treeView.setItemImage(hTreeItem, INDEX_OPEN_NODE, INDEX_OPEN_NODE);
		}
		break;

		case IDM_PROJECT_ADDFILESRECUSIVELY:
		{
			addFilesFromDirectory(hTreeItem, false);
			if (getNodeType(hTreeItem) == nodeType_folder)
				_treeView.setItemImage(hTreeItem, INDEX_OPEN_NODE, INDEX_OPEN_NODE);
		}
		break;

		case IDM_PROJECT_EDITADDFOLDERMONITOR:
		{
			addFilesFromDirectory(hTreeItem, true);
			if (getNodeType(hTreeItem) == nodeType_monitorFolderRoot)
				_treeView.setItemImage(hTreeItem, INDEX_OPEN_MONITOR, INDEX_OPEN_MONITOR);
		}
		break;

		case IDM_PROJECT_OPENWS:
		{
			if (_isDirty)
			{
				int res = ::MessageBox(_hSelf, TEXT("The current workspace was modified. Do you want to save the current project?"), TEXT("Open Workspace"), MB_YESNOCANCEL | MB_ICONQUESTION | MB_APPLMODAL);
				if (res == IDYES)
				{
					if (!saveWorkSpace())
						return;
				}
				else if (res == IDNO)
				{
					// Don't save so do nothing here
				}
				else if (res == IDCANCEL) 
				{
					// User cancels action "New Workspace" so we interrupt here
					return;
				}
			}

			FileDialog fDlg(_hSelf, ::GetModuleHandle(NULL));
			fDlg.setExtFilter(TEXT("All types"), TEXT(".*"), NULL);
			if (TCHAR *fn = fDlg.doOpenSingleFileDlg())
			{
				if (!openWorkSpace(fn))
				{
					::MessageBox(_hSelf, TEXT("The workspace could not be opened.\rIt seems the file to open is not a valid project file."), TEXT("Open Workspace"), MB_OK);
					return;
				}
			}
		}
		break;

		case IDM_PROJECT_RELOADWS:
		{
			if (_isDirty)
			{
				int res = ::MessageBox(_hSelf, TEXT("The current workspace was modified. Reloading will discard all modifications.\rDo you want to continue?"), TEXT("Reload Workspace"), MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL);
				if (res == IDYES)
				{
					// Do nothing
				}
				else if (res == IDNO)
				{
					return;
				}
			}

			if (::PathFileExists(_workSpaceFilePath.c_str()))
			{
				openWorkSpace(_workSpaceFilePath.c_str());
			}
			else
			{
				::MessageBox(_hSelf, TEXT("Cannot find the file to reload."), TEXT("Reload Workspace"), MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL);
			}
		}
		break;

		case IDM_PROJECT_SAVEWS:
			saveWorkSpace();
		break;

		case IDM_PROJECT_SAVEACOPYASWS:
		case IDM_PROJECT_SAVEASWS:
		{
			saveWorkSpaceAs(cmdID == IDM_PROJECT_SAVEACOPYASWS);
		}
		break;

		case IDM_PROJECT_DELETEFOLDER :
		{
			HTREEITEM parent = _treeView.getParent(hTreeItem);

			if (_treeView.getChildFrom(hTreeItem) != NULL)
			{
				TCHAR str2display[MAX_PATH] = TEXT("All the sub-items will be removed.\rAre you sure you want to remove this folder from the project?");
				if (::MessageBox(_hSelf, str2display, TEXT("Remove folder from project"), MB_YESNO) == IDYES)
				{
					_treeView.removeItem(hTreeItem);
					setWorkSpaceDirty(true);
				}
			}
			else
			{
				_treeView.removeItem(hTreeItem);
				setWorkSpaceDirty(true);
			}
			if (getNodeType(parent) == nodeType_folder)
				_treeView.setItemImage(parent, INDEX_CLOSED_NODE, INDEX_CLOSED_NODE);
		}
		break;

		case IDM_PROJECT_DELETEFILE :
		{
			HTREEITEM parent = _treeView.getParent(hTreeItem);

			TCHAR str2display[MAX_PATH] = TEXT("Are you sure you want to remove this file from the project?");
			if (::MessageBox(_hSelf, str2display, TEXT("Remove file from project"), MB_YESNO) == IDYES)
			{
				_treeView.removeItem(hTreeItem);
				setWorkSpaceDirty(true);
				if (getNodeType(parent) == nodeType_folder)
					_treeView.setItemImage(parent, INDEX_CLOSED_NODE, INDEX_CLOSED_NODE);
			}
		}
		break;

		case IDM_PROJECT_MODIFYFILEPATH :
		{
			FileRelocalizerDlg fileRelocalizerDlg;
			fileRelocalizerDlg.init(_hInst, _hParent);

			TCHAR textBuffer[MAX_PATH];
			TVITEM tvItem;
			tvItem.hItem = hTreeItem;
			tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
			tvItem.pszText = textBuffer;
			tvItem.cchTextMax = MAX_PATH;
			
			SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
			ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*)tvItem.lParam;
			if (!tvFileInfo.isFile())
				return;

			if (fileRelocalizerDlg.doDialog(tvFileInfo._filePath.c_str()) == 0)
			{
				generic_string newValue = fileRelocalizerDlg.getFullFilePath();
				if (tvFileInfo._filePath == newValue)
					return;

				tvFileInfo._filePath = newValue;
				TCHAR *strValueLabel = ::PathFindFileName(tvFileInfo._filePath.c_str());
				lstrcpy(textBuffer, strValueLabel);
				int iImage = ::PathFileExists(tvFileInfo._filePath.c_str()) ? INDEX_LEAF : INDEX_LEAF_INVALID;
				tvItem.iImage = tvItem.iSelectedImage = iImage;
				SendMessage(_treeView.getHSelf(), TVM_SETITEM, 0,(LPARAM)&tvItem);
				setWorkSpaceDirty(true);
			}
		}
		break;
	}
}

bool ProjectPanel::saveWorkSpaceAs(bool saveCopyAs)
{
	FileDialog fDlg(_hSelf, ::GetModuleHandle(NULL));
	fDlg.setExtFilter(TEXT("All types"), TEXT(".*"), NULL);

	if (TCHAR *fn = fDlg.doSaveDlg())
	{
		writeWorkSpace(fn);
		if (!saveCopyAs)
		{
			_workSpaceFilePath = fn;
			setWorkSpaceDirty(false);
		}
		return true;
	}
	return false;
}

void ProjectPanel::addFiles(HTREEITEM hTreeItem)
{
	FileDialog fDlg(_hSelf, ::GetModuleHandle(NULL));
	fDlg.setExtFilter(TEXT("All types"), TEXT(".*"), NULL);

	if (stringVector *pfns = fDlg.doOpenMultiFilesDlg())
	{
		size_t sz = pfns->size();
		for (size_t i = 0 ; i < sz ; ++i)
		{
			TCHAR *strValueLabel = ::PathFindFileName(pfns->at(i).c_str());
			_treeView.addItem(strValueLabel, hTreeItem, INDEX_LEAF, new ProjectPanelFileData(_directoryWatcher, strValueLabel, pfns->at(i).c_str(), nodeType_file));
		}
		_treeView.expand(hTreeItem);
		setWorkSpaceDirty(true);
	}
}

void ProjectPanel::recursiveAddFilesFrom(const TCHAR *folderPath, HTREEITEM hTreeItem, bool virtl, bool recursive)
{
	bool isInHiddenDir = false;
	generic_string dirFilter(folderPath);
	if (folderPath[lstrlen(folderPath)-1] != '\\')
		dirFilter += TEXT("\\");

	dirFilter += TEXT("*.*");
	WIN32_FIND_DATA foundData;
	std::vector<generic_string> files;

	HANDLE hFile = ::FindFirstFile(dirFilter.c_str(), &foundData);
	
	do {
		if (hFile == INVALID_HANDLE_VALUE)
			break;

		if (foundData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!isInHiddenDir && (foundData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
			{
				// do nothing
			}
			else if (recursive)
			{
				if ((lstrcmp(foundData.cFileName, TEXT("."))) && (lstrcmp(foundData.cFileName, TEXT(".."))))
				{
					generic_string pathDir(folderPath);
					if (folderPath[lstrlen(folderPath)-1] != '\\')
						pathDir += TEXT("\\");
					pathDir += foundData.cFileName;
					pathDir += TEXT("\\");
					HTREEITEM addedItem = addFolder(hTreeItem, foundData.cFileName, virtl, false, virtl ? folderPath : NULL);
					recursiveAddFilesFrom(pathDir.c_str(), addedItem, virtl, true);
				}
			}
		}
		else
		{
			files.push_back(foundData.cFileName);
		}
	} while (::FindNextFile(hFile, &foundData));
	
	for (size_t i = 0, len = files.size() ; i < len ; ++i)
	{
		generic_string pathFile(folderPath);
		if (folderPath[lstrlen(folderPath)-1] != '\\')
			pathFile += TEXT("\\");
		pathFile += files[i];
		_treeView.addItem(files[i].c_str(), hTreeItem, virtl ? INDEX_LEAF_MONITOR : INDEX_LEAF, new ProjectPanelFileData(_directoryWatcher, files[i].c_str(), pathFile.c_str(), virtl ? nodeType_monitorFile : nodeType_file ), treeviewInsertFunc);
	}

	::FindClose(hFile);
}

void ProjectPanel::addFilesFromDirectory(HTREEITEM hTreeItem, bool virtl)
{
	if (_selDirOfFilesFromDirDlg == TEXT("") && _workSpaceFilePath != TEXT(""))
	{
		TCHAR dir[MAX_PATH];
		lstrcpy(dir, _workSpaceFilePath.c_str());
		::PathRemoveFileSpec(dir);
		_selDirOfFilesFromDirDlg = dir;
	}
	generic_string dirPath;
	if (_selDirOfFilesFromDirDlg != TEXT(""))
		dirPath = getFolderName(_hSelf, _selDirOfFilesFromDirDlg.c_str());
	else
		dirPath = getFolderName(_hSelf);

	if (dirPath != TEXT(""))
	{
		if (virtl)
		{
			generic_string newFolderLabel(dirPath);
			const size_t lastSlashIdx = newFolderLabel.find_last_of(TEXT("\\/"));
			if (std::string::npos != lastSlashIdx)
			{
				newFolderLabel.erase(0, lastSlashIdx + 1);
				if( newFolderLabel.empty() ) // drive added
				{
					newFolderLabel = dirPath;
					newFolderLabel.erase(lastSlashIdx);
				}
			}
			hTreeItem = addFolder(hTreeItem, newFolderLabel.c_str(), true, true, dirPath.c_str());
		}

		recursiveAddFilesFrom(dirPath.c_str(), hTreeItem, virtl, !virtl);
		if (!virtl)
			_treeView.expand(hTreeItem);
		setWorkSpaceDirty(true);
		_selDirOfFilesFromDirDlg = dirPath;
	}
}

INT_PTR CALLBACK FileRelocalizerDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM) 
{
	switch (Message)
	{
		case WM_INITDIALOG :
		{
			goToCenter();
			::SetDlgItemText(_hSelf, IDC_EDIT_FILEFULLPATHNAME, _fullFilePath.c_str());
			return TRUE;
		}
		case WM_COMMAND : 
		{
			switch (wParam)
			{
				case IDOK :
				{
					TCHAR textBuf[MAX_PATH];
					::GetDlgItemText(_hSelf, IDC_EDIT_FILEFULLPATHNAME, textBuf, MAX_PATH);
					_fullFilePath = textBuf;
					::EndDialog(_hSelf, 0);
				}
				return TRUE;

				case IDCANCEL :
					::EndDialog(_hSelf, -1);
				return TRUE;

				default:
					return FALSE;
			}
		}
		default :
			return FALSE;
	}
}

int FileRelocalizerDlg::doDialog(const TCHAR *fn, bool isRTL) 
{
	_fullFilePath = fn;

	if (isRTL)
	{
		DLGTEMPLATE *pMyDlgTemplate = NULL;
		HGLOBAL hMyDlgTemplate = makeRTLResource(IDD_FILERELOCALIZER_DIALOG, &pMyDlgTemplate);
		int result = ::DialogBoxIndirectParam(_hInst, pMyDlgTemplate, _hParent,  dlgProc, (LPARAM)this);
		::GlobalFree(hMyDlgTemplate);
		return result;
	}
	return ::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_FILERELOCALIZER_DIALOG), _hParent,  dlgProc, (LPARAM)this);
}

ProjectPanelDirectory::ProjectPanelDirectory(ProjectPanel* projectPanel, HTREEITEM hItem) 
	: Directory()
	, _projectPanel(projectPanel)
	, _treeView(&projectPanel->getTreeView())
	, _hItem(hItem)
	, _wasInitiallyEmpty(false)
{
	TCHAR textBuffer[MAX_PATH];
	TVITEM tvItem;
	tvItem.mask = TVIF_TEXT | TVIF_PARAM;
	tvItem.pszText = textBuffer;
	tvItem.cchTextMax = MAX_PATH;

	tvItem.hItem = hItem;
	SendMessage(_treeView->getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
	ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*) tvItem.lParam;
	assert (tvFileInfo.isFolderMonitorRoot() || tvFileInfo.isFolderMonitor());
	if (!tvFileInfo.isFolderMonitorRoot() && !tvFileInfo.isFolderMonitor())
		return;
	_exists = PathFileExists(tvFileInfo._filePath.c_str()) != 0;
	if (_exists)
		_path = tvFileInfo._filePath;

    for (HTREEITEM hItemNode = _treeView->getChildFrom(hItem);
		hItemNode != NULL;
		hItemNode = _treeView->getNextSibling(hItemNode))
	{
		tvItem.hItem = hItemNode;
		SendMessage(_treeView->getHSelf(), TVM_GETITEM, 0,(LPARAM)&tvItem);
		ProjectPanelFileData& tvFileInfo = *(ProjectPanelFileData*) tvItem.lParam;

		generic_string filename(tvFileInfo._filePath);
		const size_t lastSlashIdx = filename.find_last_of(TEXT("\\/"));
		if (std::string::npos != lastSlashIdx)
			filename.erase(0, lastSlashIdx + 1);

		if (tvFileInfo.isFileMonitor())
		{
			_files.insert(filename);
			_fileMap[filename] = hItemNode;
		}
		else if (tvFileInfo.isFolderMonitorRoot() || tvFileInfo.isFolderMonitor())
		{
			_dirs.insert(filename);
			_dirMap[filename] = hItemNode;
		}
		else
		{
			assert(0);
		}
	}

}


void ProjectPanelDirectory::onBeginSynchronize(const Directory& other)
{
	_wasInitiallyEmpty = empty();
}

void ProjectPanelDirectory::onDirAdded(const generic_string& name)
{
	_projectPanel->addFolder(_hItem, name.c_str(), true, false, (_path + TEXT("\\") + name).c_str(), !_wasInitiallyEmpty );

}

void ProjectPanelDirectory::onDirRemoved(const generic_string& name)
{
	_treeView->removeItem(_dirMap[name]);
}

void ProjectPanelDirectory::onFileAdded(const generic_string& name)
{
	_treeView->addItem(name.c_str(), _hItem, INDEX_LEAF_MONITOR, new ProjectPanelFileData(_projectPanel->_directoryWatcher, name.c_str(), (_path + TEXT("\\") + name).c_str(), nodeType_monitorFile ), _wasInitiallyEmpty ? NULL : treeviewInsertFunc);

}

void ProjectPanelDirectory::onFileRemoved(const generic_string& name)
{
	_treeView->removeItem(_fileMap[name]);
}

#pragma warning( pop )

