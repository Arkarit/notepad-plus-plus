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

enum { 
	IndexCleanRoot,
	IndexDirtyRoot,
	IndexProject,
	IndexOpenNode,
	IndexClosedNode,
	IndexLeaf,
	IndexLeafInvalid,
	IndexOpenDir,
	IndexClosedDir,
	IndexOpenBasedir,
	IndexClosedBasedir,
	IndexOfflineBasedir,
	IndexLeafDirFile,
};


#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))

#define PROJECTPANEL_FILTERS_MAXLENGTH 2048


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

			SendMessage(_hToolbarMenu, TB_BUTTONSTRUCTSIZE, static_cast<WPARAM>(sizeof(TBBUTTON)), 0);
			SendMessage(_hToolbarMenu, TB_ADDBUTTONS,       static_cast<WPARAM>(sizeof(tbButtons) / sizeof(TBBUTTON)),       reinterpret_cast<LPARAM>(&tbButtons));
			SendMessage(_hToolbarMenu, TB_AUTOSIZE, 0, 0); 
			ShowWindow(_hToolbarMenu, SW_SHOW);

			_treeView.init(_hInst, _hSelf, ID_PROJECTTREEVIEW);
			_treeView.setListener(this);

			_directoryWatcher.setWindow(_treeView.getHSelf());
			_directoryWatcher.startThread();

			const std::vector<int> imageIndices =
			{ 
				IDI_PROJECT_WORKSPACE, 
			    IDI_PROJECT_WORKSPACEDIRTY, 
				IDI_PROJECT_PROJECT, 
				IDI_PROJECT_FOLDEROPEN, 
				IDI_PROJECT_FOLDERCLOSE, 
				IDI_PROJECT_FILE, 
				IDI_PROJECT_FILEINVALID, 
				IDI_PROJECT_DIROPEN, 
				IDI_PROJECT_DIRCLOSE, 
				IDI_PROJECT_BASEDIROPEN, 
				IDI_PROJECT_BASEDIRCLOSE, 
				IDI_PROJECT_BASEDIROFFLINE, 
				IDI_PROJECT_DIRFILE,
			};

			setImageList(imageIndices);

			_treeView.addCanNotDropInList(IndexLeaf);
			_treeView.addCanNotDropInList(IndexLeafInvalid);
			_treeView.addCanNotDropInList(IndexOpenDir);
			_treeView.addCanNotDropInList(IndexClosedDir);
			_treeView.addCanNotDropInList(IndexOpenBasedir);
			_treeView.addCanNotDropInList(IndexClosedBasedir);
			_treeView.addCanNotDropInList(IndexOfflineBasedir);
			_treeView.addCanNotDropInList(IndexLeafDirFile);

			_treeView.addCanNotDragOutList(IndexCleanRoot);
			_treeView.addCanNotDragOutList(IndexDirtyRoot);
			_treeView.addCanNotDragOutList(IndexProject);
			_treeView.addCanNotDragOutList(IndexOpenDir);
			_treeView.addCanNotDragOutList(IndexClosedDir);
			_treeView.addCanNotDragOutList(IndexOpenBasedir);
			_treeView.addCanNotDragOutList(IndexClosedBasedir);
			_treeView.addCanNotDragOutList(IndexOfflineBasedir);
			_treeView.addCanNotDragOutList(IndexLeafDirFile);

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
			_treeView.setListener(NULL);
			_directoryWatcher.stopThread();
			_directoryWatcher.removeAllDirs();
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
	generic_string edit_addDirectory = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_EDITADDDIRECTORY, PM_EDITADDDIRECTORY);
	generic_string edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_DELETEFOLDER, PM_EDITREMOVE);
	generic_string edit_setfilter = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_SETFILTERS, PM_SETFILTERS);
	generic_string edit_refresh = pNativeSpeaker->getProjectPanelLangMenuStr("ProjectMenu", IDM_PROJECT_REFRESH, PM_REFRESH);

	_hProjectMenu = ::CreatePopupMenu();
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEUP, edit_moveup.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEDOWN, edit_movedown.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, UINT(-1), 0);
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_RENAME, edit_rename.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_NEWFOLDER, edit_addfolder.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILES, edit_addfiles.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILESRECUSIVELY, edit_addfilesRecursive.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_EDITADDDIRECTORY, edit_addDirectory.c_str());
	::InsertMenu(_hProjectMenu, 0, MF_BYCOMMAND, IDM_PROJECT_DELETEFOLDER, edit_remove.c_str());

	edit_moveup = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_MOVEUP, PM_MOVEUPENTRY);
	edit_movedown = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_MOVEDOWN, PM_MOVEDOWNENTRY);
	edit_rename = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_RENAME, PM_EDITRENAME);
	edit_addfolder = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_NEWFOLDER, PM_EDITNEWFOLDER);
	edit_addfiles = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_ADDFILES, PM_EDITADDFILES);
	edit_addfilesRecursive = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_ADDFILESRECUSIVELY, PM_EDITADDFILESRECUSIVELY);
	edit_addDirectory = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_EDITADDDIRECTORY, PM_EDITADDDIRECTORY);
	edit_remove = pNativeSpeaker->getProjectPanelLangMenuStr("FolderMenu", IDM_PROJECT_DELETEFOLDER, PM_EDITREMOVE);

	_hFolderMenu = ::CreatePopupMenu();
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEUP,        edit_moveup.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEDOWN,      edit_movedown.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, UINT(-1), 0);
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_RENAME,        edit_rename.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_NEWFOLDER,     edit_addfolder.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILES,      edit_addfiles.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_ADDFILESRECUSIVELY, edit_addfilesRecursive.c_str());
	::InsertMenu(_hFolderMenu, 0, MF_BYCOMMAND, IDM_PROJECT_EDITADDDIRECTORY, edit_addDirectory.c_str());
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

	_hDirectoryMenu = ::CreatePopupMenu();
	::InsertMenu(_hDirectoryMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEUP, edit_moveup.c_str());
	::InsertMenu(_hDirectoryMenu, 0, MF_BYCOMMAND, IDM_PROJECT_MOVEDOWN, edit_movedown.c_str());
	::InsertMenu(_hDirectoryMenu, 0, MF_BYCOMMAND, IDM_PROJECT_DELETEFILE, edit_remove.c_str());
	::InsertMenu(_hDirectoryMenu, 0, MF_BYCOMMAND, IDM_PROJECT_SETFILTERS, edit_setfilter.c_str());
	::InsertMenu(_hDirectoryMenu, 0, MF_BYCOMMAND, IDM_PROJECT_RENAME, edit_rename.c_str());
	::InsertMenu(_hDirectoryMenu, 0, MF_BYCOMMAND, IDM_PROJECT_REFRESH, edit_refresh.c_str());

}


void ProjectPanel::setImageList(const std::vector<int>& imageIndices) 
{
	const int nbBitmaps = imageIndices.size();

	// Creation of image list
	if ((_hImaLst = ImageList_Create(CX_BITMAP, CY_BITMAP, ILC_COLOR32 | ILC_MASK, nbBitmaps, 0)) == NULL) 
		throw std::runtime_error("ProjectPanel::setImageList : ImageList_Create() return NULL");

	for (auto& idx : imageIndices)
		setImageListImage(idx);


	assert (ImageList_GetImageCount(_hImaLst) == nbBitmaps);

	// Set image list to the tree view
	TreeView_SetImageList(_treeView.getHSelf(), _hImaLst, TVSIL_NORMAL);

}


void ProjectPanel::setImageListImage(int idx)
{
	HBITMAP hbmp;
	const COLORREF maskColour = RGB(192, 192, 192);

	// Add the bmp to the list
	hbmp = LoadBitmap(_hInst, MAKEINTRESOURCE(idx));
	if (hbmp == NULL)
		throw std::logic_error("ProjectPanel::setImageListImage :  LoadBitmap() returned NULL");

	if (ImageList_AddMasked(_hImaLst, hbmp, maskColour) == -1)
		throw std::runtime_error("ProjectPanel::setImageListImage :  ImageList_AddMasked() failed");

	if (!DeleteObject(hbmp))
		throw std::logic_error("ProjectPanel::setImageListImage :  DeleteObject() failed");

}

void ProjectPanel::destroyMenus()
{
	::DestroyMenu(_hWorkSpaceMenu);
	::DestroyMenu(_hProjectMenu);
	::DestroyMenu(_hFolderMenu);
	::DestroyMenu(_hFileMenu);
	::DestroyMenu(_hDirectoryMenu);
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
	HTREEITEM rootItem = _treeView.addItem(workspace.c_str(), TVI_ROOT, IndexCleanRoot, new ProjectPanelData(_directoryWatcher, workspace.c_str(), NULL, nodeType_root));

	for ( ; childNode ; childNode = childNode->NextSibling(TEXT("Project")))
	{
		const TCHAR* name = (childNode->ToElement())->Attribute(TEXT("name"));
		HTREEITEM projectItem = _treeView.addItem(name, rootItem, IndexProject, new ProjectPanelData(_directoryWatcher, name, NULL, nodeType_project));
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
	_treeView.addItem(workspace.c_str(), TVI_ROOT, IndexCleanRoot, new ProjectPanelData(_directoryWatcher, workspace.c_str(), NULL, nodeType_root));
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
        SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
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
		SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
		ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);

		if (data.isFile())
		{
			generic_string newFn = getRelativePath(data._filePath, fn2write);
			TiXmlNode *fileLeaf = node->InsertEndChild(TiXmlElement(TEXT("File")));
			fileLeaf->ToElement()->SetAttribute(TEXT("name"), newFn.c_str());
		}
		else if (data.isBaseDirectory())
		{
			generic_string newFn = getRelativePath(data._filePath, fn2write);
			TiXmlNode *fileLeaf = node->InsertEndChild(TiXmlElement(TEXT("Directory")));
			fileLeaf->ToElement()->SetAttribute(TEXT("name"), newFn.c_str());
			generic_string filters = stringJoin(data._filters, TEXT(";"), true);
			if (!filters.empty())
				fileLeaf->ToElement()->SetAttribute(TEXT("filters"), filters.c_str());
			if (!data._userLabel.empty())
				fileLeaf->ToElement()->SetAttribute(TEXT("userLabel"), data._userLabel.c_str());
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
			HTREEITEM addedItem = _treeView.addItem(name, hParentItem, IndexClosedNode, new ProjectPanelData(_directoryWatcher, name, NULL, nodeType_folder));
			if (!childNode->NoChildren())
			{
				bool isOK = buildTreeFrom(childNode, addedItem);
				if (!isOK)
					return false;
			}
		}
		if (lstrcmp(TEXT("Directory"), v) == 0)
		{

			const TCHAR *strLabel = (childNode->ToElement())->Attribute(TEXT("userLabel"));
			generic_string newFolderLabel;
			
			const TCHAR *strValue = (childNode->ToElement())->Attribute(TEXT("name"));
			generic_string fullPath = getAbsoluteFilePath(strValue);

			std::vector<generic_string> filters;
			const TCHAR *strFilters = (childNode->ToElement())->Attribute(TEXT("filters"));
			if (strFilters)
			{
				generic_string filter(strFilters);
				filters = stringSplit(filter, TEXT(";"), true);
			}

			if (strLabel)
			{
				newFolderLabel = strLabel;
			}
			else
			{

				newFolderLabel = buildDirectoryName(fullPath, filters);
			}


			HTREEITEM hNewDirectory = addDirectory(hParentItem, newFolderLabel.c_str(), true, fullPath.c_str(), &filters);
			if (strLabel)
			{
				TVITEM tvItem;
				tvItem.mask = TVIF_PARAM;
				tvItem.hItem = hNewDirectory;
				::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
				ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);
				data._userLabel = strLabel;
			}

		}
		else if (lstrcmp(TEXT("File"), v) == 0)
		{
			const TCHAR *strValue = (childNode->ToElement())->Attribute(TEXT("name"));
			generic_string fullPath = getAbsoluteFilePath(strValue);
			TCHAR *strValueLabel = ::PathFindFileName(strValue);
			int iImage = ::PathFileExists(fullPath.c_str())?IndexLeaf:IndexLeafInvalid;
			_treeView.addItem(strValueLabel, hParentItem, iImage, new ProjectPanelData(_directoryWatcher, strValueLabel, fullPath.c_str(), nodeType_file));
		}
	}
	return true;
}

void ProjectPanel::rebuildDirectoryTree(HTREEITEM hParentItem, const ProjectPanelData& data)
{
	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hParentItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));

	int iImg;
	if (data.isBaseDirectory())
	{
		if (!::PathFileExists(data._filePath.c_str()))
		{
			iImg = IndexOfflineBasedir;
			_treeView.removeAllChildren(hParentItem);
		}
		else
		{
			iImg = (tvItem.state & TVIS_EXPANDED) ? IndexOpenBasedir : IndexClosedBasedir;
		}
	}
	else if (data.isDirectory())
	{
		if (!::PathFileExists(data._filePath.c_str()))
		{
			_treeView.removeItem(hParentItem);
			return;
		}
		else
		{
			iImg = (tvItem.state & TVIS_EXPANDED) ? IndexOpenDir : IndexClosedDir;
		}
	}
	else
	{
		assert(0);
		return;
	}

	removeDummies(hParentItem);
	TreeUpdaterDirectory currDir( this, hParentItem );
	Directory newDir(data._filePath, data._filters);
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
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));

	NodeType nType = getNodeType(tvItem.hItem);
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);

	if ((nType == nodeType_file || nType == nodeType_dirFile) && (data.isFile() || data.isDirectoryFile()))
	{
		tvItem.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		if (::PathFileExists(data._filePath.c_str()))
		{
			::SendMessage(_hParent, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(data._filePath.c_str()));
			tvItem.iImage = data.isDirectoryFile() ? IndexLeafDirFile : IndexLeaf;
			tvItem.iSelectedImage = data.isDirectoryFile() ? IndexLeafDirFile : IndexLeaf;
		}
		else
		{
			tvItem.iImage = IndexLeafInvalid;
			tvItem.iSelectedImage = IndexLeafInvalid;
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
	else if (nodeType == nodeType_baseDir)
		hMenu = _hDirectoryMenu;
	else if (nodeType == nodeType_dir || nodeType == nodeType_dirFile)
		hMenu = NULL; // currently no context menu for monitored files
	else //nodeType_file
		hMenu = _hFileMenu;
	return hMenu;

}

void ProjectPanel::onTreeItemAdded(bool afterClone, HTREEITEM hItem, TreeViewData* newData)
{
	afterClone;

	ProjectPanelData* tvInfo = static_cast<ProjectPanelData*> (newData);
	tvInfo->setItem( hItem );
	if (tvInfo->isBaseDirectory())
		tvInfo->watchDir(true);
}

void ProjectPanel::onMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	wParam;

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
				SendMessage(hwnd, TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
				treeItemChanged(hItem, (TreeViewData*) tvItem.lParam);
			}
		}
		break;
	}

}

void ProjectPanel::treeItemChanged(HTREEITEM hTreeItem, TreeViewData* tvdata)
{

	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hTreeItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));

	ProjectPanelData& data = *static_cast<ProjectPanelData*>(tvdata);
	int iImg;
	switch (data._nodeType)
	{
		case nodeType_root:
			iImg = _isDirty ? IndexDirtyRoot : IndexCleanRoot;
			break;
		case nodeType_project:
			iImg = IndexProject;
			break;
		case nodeType_folder:
			iImg = (tvItem.state & TVIS_EXPANDED) ? IndexOpenNode : IndexClosedNode;
			break;
		case nodeType_file:
			iImg = ::PathFileExists(data._filePath.c_str()) ? IndexLeaf : IndexLeafInvalid;
			break;
		case nodeType_dirFile:
			iImg = IndexLeafDirFile;
			break;
		case nodeType_baseDir:
		case nodeType_dir:
			rebuildDirectoryTree(hTreeItem, data);
			return;
		default:
			return;
	}
	_treeView.setItemImage(hTreeItem, iImg, iImg);

}

generic_string ProjectPanel::buildDirectoryName(const generic_string& filePath, const std::vector<generic_string>& filters)
{
	generic_string result(filePath);

	const size_t lastSlashIdx = result.find_last_of(TEXT("\\/"));
	if (std::string::npos != lastSlashIdx)
	{
		result.erase(0, lastSlashIdx + 1);
		if( result.empty() ) // drive
		{
			result = filePath;
			result.erase(lastSlashIdx);
		}
	}

	if (!filters.empty())
	{
		result += TEXT(" (");
		result += stringJoin(filters, TEXT(";"), true);
		result += TEXT(")");
	}

	return result;

}

void ProjectPanel::itemVisibilityChanges(HTREEITEM hItem, bool visible)
{
	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);
	bool expanded = (tvItem.state & TVIS_EXPANDED) != 0;
	if (data.isDirectory())
	{
		if (visible && expanded)
			data.watchDir(true);
		if (!visible || !expanded)
			data.watchDir(false);
	}

	for (HTREEITEM hChildItem = _treeView.getChildFrom(hItem);
    hChildItem != NULL;
    hChildItem = _treeView.getNextSibling(hChildItem))
    {
		itemVisibilityChanges(hChildItem, visible);
	}

}

void ProjectPanel::expandOrCollapseDirectory(bool expand, HTREEITEM hItem)
{

	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);
	if (!(data.isDirectory() || data.isBaseDirectory()) )
		return;

	if (expand)
	{
		removeDummies(hItem);
		if (data.isDirectory())
			data.watchDir(true);
	}
	else
	{
		if (data.isDirectory())
			data.watchDir(false);
	}

	for (HTREEITEM hChildItem = _treeView.getChildFrom(hItem);
    hChildItem != NULL;
    hChildItem = _treeView.getNextSibling(hChildItem))
    {
		itemVisibilityChanges(hChildItem, expand);
	}

	_directoryWatcher.update();

}

bool ProjectPanel::setFilters(const std::vector<generic_string>& filters, HTREEITEM hItem)
{
	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);

	if (data.isDirectory() || data.isBaseDirectory())
	{
		if (data.isBaseDirectory() && data._filters == filters)
			return false;

		data._filters = filters;
	}

	if (data.isWatching())
	{
		data.watchDir(true);
	}

	for (HTREEITEM hItemNode = _treeView.getChildFrom(hItem);
		hItemNode != NULL;
		hItemNode = _treeView.getNextSibling(hItemNode))
	{
		setFilters(filters,hItemNode);
	}

	updateBaseDirName(hItem);
	return true;
}

void ProjectPanel::updateBaseDirName(HTREEITEM hItem)
{
	TCHAR textBuffer[MAX_PATH];
	TVITEM tvItem;
	tvItem.mask = TVIF_TEXT | TVIF_PARAM;
	tvItem.pszText = textBuffer;
	tvItem.cchTextMax = MAX_PATH;
	tvItem.hItem = hItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);

	if (data.isDirectory() || data.isDirectoryFile())
	{
		updateBaseDirName(_treeView.getParent(hItem));
	}
	else if (data.isBaseDirectory())
	{
		if (data._userLabel.empty())
		{
			generic_string newFolderLabel = buildDirectoryName(data._filePath, data._filters);
			wcsncpy(tvItem.pszText,newFolderLabel.c_str(), MAX_PATH-1);
			tvItem.mask = TVIF_TEXT;
			::SendMessage(_treeView.getHSelf(), TVM_SETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
		}
	}
}

void ProjectPanel::removeDummies(HTREEITEM hTreeItem)
{
	HTREEITEM hItemNode = _treeView.getChildFrom(hTreeItem);
	if (hItemNode == NULL)
		return;

	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hItemNode;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);

	if (data.isDummy())
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
				NodeType nt = getNodeType(tvnotif->item.hItem);
				if (!tvnotif->item.pszText && nt != nodeType_baseDir)
					return;
				if (nt == nodeType_root || nt == nodeType_dirFile || nt == nodeType_dir)
					return;

				ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvnotif->item.lParam);

				// Processing for only File case
				if (nt == nodeType_file) 
				{
					// Get the old label
					tvItem.hItem = _treeView.getSelection();
					::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
					size_t len = lstrlen(tvItem.pszText);

					// Find the position of old label in File path
					generic_string &filePath = data._filePath;
					size_t found = filePath.rfind(tvItem.pszText);

					// If found the old label, replace it with the modified one
					if (found != generic_string::npos)
						filePath.replace(found, len, tvnotif->item.pszText);

					// Check the validity of modified file path
					tvItem.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
					if (::PathFileExists(filePath.c_str()))
					{
						tvItem.iImage = data.isDirectoryFile() ? IndexLeafDirFile : IndexLeaf;
						tvItem.iSelectedImage = data.isDirectoryFile() ? IndexLeafDirFile : IndexLeaf;
					}
					else
					{
						tvItem.iImage = IndexLeafInvalid;
						tvItem.iSelectedImage = IndexLeafInvalid;
					}
					TreeView_SetItem(_treeView.getHSelf(), &tvItem);
				}
				else if (nt == nodeType_baseDir)
				{
					ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvnotif->item.lParam);

					// a text was added for base directory - give it its own name
					if (tvnotif->item.pszText && *tvnotif->item.pszText)
					{
						data._userLabel = tvnotif->item.pszText;
					}
					else
					{
						data._userLabel.clear();
						generic_string newFolderLabel = buildDirectoryName(data._filePath, data._filters);
						wcsncpy(tvItem.pszText,newFolderLabel.c_str(), MAX_PATH-1);
						tvItem.hItem = tvnotif->item.hItem;
						tvItem.mask = TVIF_TEXT;
						::SendMessage(_treeView.getHSelf(), TVM_SETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
						break;
					}
				}

				// For File, Folder and Project
				::SendMessage(_treeView.getHSelf(), TVM_SETITEM, 0,reinterpret_cast<LPARAM>(&(tvnotif->item)));
				setWorkSpaceDirty(true);
			}
			break;

			case TVN_GETINFOTIP:
			{
				LPNMTVGETINFOTIP lpGetInfoTip = (LPNMTVGETINFOTIP)notification;

				if (_treeView.getRoot() == lpGetInfoTip->hItem)
				{
					_infotipStr = _workSpaceFilePath;
				}
				else
				{
					_infotipStr = reinterpret_cast<ProjectPanelData *>(lpGetInfoTip->lParam)->_filePath;
					if (_infotipStr.empty())
						return;

					const std::vector<generic_string>* filters = getFilters(lpGetInfoTip->hItem);
					if (filters && !filters->empty())
					{
						_infotipStr += TEXT(" (");
						_infotipStr += stringJoin(*filters, TEXT(";"), true);
						_infotipStr += TEXT(")");
					}
				}
				lpGetInfoTip->pszText = (LPTSTR)_infotipStr.c_str();
				lpGetInfoTip->cchTextMax = _infotipStr.size();
			}
			break;

			case TVN_KEYDOWN:
			{
				//tvItem.hItem = _treeView.getSelection();
				//::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>&tvItem);
				LPNMTVKEYDOWN ptvkd = (LPNMTVKEYDOWN)notification;
				
				if (ptvkd->wVKey == VK_DELETE)
				{
					HTREEITEM hItem = _treeView.getSelection();
					NodeType nType = getNodeType(hItem);
					if (nType == nodeType_project || nType == nodeType_folder || nType == nodeType_baseDir)
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
						_treeView.setItemImage(nmtv->itemNew.hItem, IndexClosedNode, IndexClosedNode);
					}
					else if (nmtv->action == TVE_EXPAND)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, IndexOpenNode, IndexOpenNode);
					}
				}
				else if (getNodeType(nmtv->itemNew.hItem) == nodeType_dir)
				{
					if (nmtv->action == TVE_COLLAPSE)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, IndexClosedDir, IndexClosedDir);
					}
					else if (nmtv->action == TVE_EXPAND)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, IndexOpenDir, IndexOpenDir);
					}
				}
				else if (getNodeType(nmtv->itemNew.hItem) == nodeType_baseDir)
				{
					if (nmtv->action == TVE_COLLAPSE)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, IndexClosedBasedir, IndexClosedBasedir);
					}
					else if (nmtv->action == TVE_EXPAND)
					{
						_treeView.setItemImage(nmtv->itemNew.hItem, IndexOpenBasedir, IndexOpenBasedir);
					}
				}

				expandOrCollapseDirectory(nmtv->action == TVE_EXPAND, nmtv->itemNew.hItem);

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
	int iImg = _isDirty?IndexDirtyRoot:IndexCleanRoot;
	_treeView.setItemImage(_treeView.getRoot(), iImg, iImg);
}

ProjectPanel::NodeType ProjectPanel::getNodeType(HTREEITEM hItem) const
{
	TVITEM tvItem;
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_PARAM;
	SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);
	return data._nodeType;
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
	SendMessage(_hToolbarMenu, TB_GETITEMRECT, iButton, reinterpret_cast<LPARAM>(&btnRect));

	p.x = btnRect.left;
	p.y = btnRect.top + btnRect.bottom;
	ClientToScreen(_hToolbarMenu, &p);
	return p;
}


const std::vector<generic_string>* ProjectPanel::getFilters(HTREEITEM hItem)
{
	TVITEM tvItem;
	tvItem.mask = TVIF_PARAM;
	tvItem.hItem = hItem;
	::SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0, reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);

	if (data.isBaseDirectory())
		return &data._filters;
	else if (data.isDirectory())
		return getFilters(_treeView.getParent(hItem));
	else return NULL;
}

HTREEITEM ProjectPanel::addFolder(HTREEITEM hTreeItem, const TCHAR *folderName)
{
	HTREEITEM addedItem = _treeView.addItem(folderName, hTreeItem, IndexClosedNode, new ProjectPanelData(_directoryWatcher, folderName, NULL, nodeType_folder));
	
	TreeView_Expand(_treeView.getHSelf(), hTreeItem, TVE_EXPAND);
	TreeView_EditLabel(_treeView.getHSelf(), addedItem);
	if (getNodeType(hTreeItem) == nodeType_folder)
		_treeView.setItemImage(hTreeItem, IndexOpenNode, IndexOpenNode);

	return addedItem;
}

HTREEITEM ProjectPanel::addDirectory(HTREEITEM hTreeItem, const TCHAR *folderName, bool root, const TCHAR *monitorPath, const std::vector<generic_string>* filters)
{
	NodeType nodeType(nodeType_dir);
	int iconindex = IndexClosedDir;

	if (root)
	{
		nodeType = nodeType_baseDir;
		iconindex = ::PathFileExists(monitorPath) ? IndexClosedBasedir : IndexOfflineBasedir;
	}

	const std::vector<generic_string> dummy;
	const std::vector<generic_string>* finalFilters = filters ? filters : getFilters(hTreeItem);
	if (finalFilters == nullptr)
		finalFilters = &dummy;
	

	HTREEITEM addedItem = _treeView.addItem(folderName, hTreeItem, iconindex, new ProjectPanelData(_directoryWatcher, folderName, monitorPath, nodeType, *finalFilters));

	_treeView.addItem( TEXT(""), addedItem, IndexLeafDirFile, new ProjectPanelData(_directoryWatcher,TEXT(""), TEXT(""), nodeType_dummy ));

	if (getNodeType(hTreeItem) != nodeType_baseDir && getNodeType(hTreeItem) != nodeType_dir)
	{
		TreeView_Expand(_treeView.getHSelf(), hTreeItem, TVE_EXPAND);
	}

	if (getNodeType(hTreeItem) == nodeType_baseDir)
		_treeView.setItemImage(hTreeItem, IndexOpenBasedir, IndexOpenBasedir);

	return addedItem;
}

HTREEITEM ProjectPanel::addDirectory(HTREEITEM hTreeItem)
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
		generic_string newFolderLabel = buildDirectoryName(dirPath);
		hTreeItem = addDirectory(hTreeItem, newFolderLabel.c_str(), true, dirPath.c_str());
		setWorkSpaceDirty(true);
		_selDirOfFilesFromDirDlg = dirPath;
	}

	return hTreeItem;
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
			HTREEITEM addedItem = _treeView.addItem(newProjectLabel.c_str(),  root, IndexProject, new ProjectPanelData(_directoryWatcher, newProjectLabel.c_str(), NULL, nodeType_project));
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

		case IDM_PROJECT_REFRESH :
			_directoryWatcher.forceUpdateAll();
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
				_treeView.setItemImage(hTreeItem, IndexOpenNode, IndexOpenNode);
		}
		break;

		case IDM_PROJECT_ADDFILESRECUSIVELY:
		{
			addFilesFromDirectory(hTreeItem);
			if (getNodeType(hTreeItem) == nodeType_folder)
				_treeView.setItemImage(hTreeItem, IndexOpenNode, IndexOpenNode);
		}
		break;

		case IDM_PROJECT_EDITADDDIRECTORY:
		{
			hTreeItem = addDirectory(hTreeItem);
			_treeView.setItemImage(hTreeItem, IndexOpenBasedir, IndexOpenBasedir);
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
				_treeView.setItemImage(parent, IndexClosedNode, IndexClosedNode);
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
					_treeView.setItemImage(parent, IndexClosedNode, IndexClosedNode);
			}
		}
		break;

		case IDM_PROJECT_SETFILTERS :
		{
			FilterDlg filterDlg;
			filterDlg.init(_hInst, _hParent);
			if (filterDlg.doDialog() == 0)
			{
				if (setFilters(filterDlg.getFilters(), hTreeItem))
					setWorkSpaceDirty(true);
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
			
			SendMessage(_treeView.getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
			ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);
			if (!data.isFile())
				return;

			if (fileRelocalizerDlg.doDialog(data._filePath.c_str()) == 0)
			{
				generic_string newValue = fileRelocalizerDlg.getFullFilePath();
				if (data._filePath == newValue)
					return;

				data._filePath = newValue;
				TCHAR *strValueLabel = ::PathFindFileName(data._filePath.c_str());
				lstrcpy(textBuffer, strValueLabel);
				int iImage = ::PathFileExists(data._filePath.c_str()) ? IndexLeaf : IndexLeafInvalid;
				tvItem.iImage = tvItem.iSelectedImage = iImage;
				SendMessage(_treeView.getHSelf(), TVM_SETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
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
			_treeView.addItem(strValueLabel, hTreeItem, IndexLeaf, new ProjectPanelData(_directoryWatcher, strValueLabel, pfns->at(i).c_str(), nodeType_file));
		}
		_treeView.expand(hTreeItem);
		setWorkSpaceDirty(true);
	}
}

void ProjectPanel::recursiveAddFilesFrom(const TCHAR *folderPath, HTREEITEM hTreeItem)
{
	bool isRecursive = true;
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

		if ((foundData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if (!isInHiddenDir && (foundData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
			{
				// do nothing
			}
			else if (isRecursive)
			{
				if ((lstrcmp(foundData.cFileName, TEXT("."))) && (lstrcmp(foundData.cFileName, TEXT(".."))))
				{
					generic_string pathDir(folderPath);
					if (folderPath[lstrlen(folderPath)-1] != '\\')
						pathDir += TEXT("\\");
					pathDir += foundData.cFileName;
					pathDir += TEXT("\\");
					HTREEITEM addedItem = addFolder(hTreeItem, foundData.cFileName);
					recursiveAddFilesFrom(pathDir.c_str(), addedItem);
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
		_treeView.addItem(files[i].c_str(), hTreeItem, IndexLeaf, new ProjectPanelData(_directoryWatcher, files[i].c_str(), pathFile.c_str(), nodeType_file) );
	}

	::FindClose(hFile);
}

void ProjectPanel::addFilesFromDirectory(HTREEITEM hTreeItem)
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
		recursiveAddFilesFrom(dirPath.c_str(), hTreeItem);
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
		int result = ::DialogBoxIndirectParam(_hInst, pMyDlgTemplate, _hParent,  dlgProc, reinterpret_cast<LPARAM>(this));
		::GlobalFree(hMyDlgTemplate);
		return result;
	}
	return ::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_FILERELOCALIZER_DIALOG), _hParent,  dlgProc, reinterpret_cast<LPARAM>(this));
}

INT_PTR CALLBACK FilterDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM) 
{
	switch (Message)
	{
		case WM_INITDIALOG :
		{
			NppParameters *nppParams = NppParameters::getInstance();
			FindHistory & findHistory = nppParams->getFindHistory();

			fillComboHistory(IDD_PROJECTPANEL_FILTERS_COMBO, findHistory._findHistoryFilters);
			goToCenter();
			return TRUE;
		}
		case WM_COMMAND : 
		{
			switch (wParam)
			{
				case IDOK :
				{
					_filters.clear();
					const int filterSize = 256;
					TCHAR tfilters[filterSize + 1];
					tfilters[filterSize] = '\0';
					::GetDlgItemText(_hSelf, IDD_PROJECTPANEL_FILTERS_COMBO, tfilters, filterSize);
					addText2Combo(tfilters, ::GetDlgItem(_hSelf, IDD_PROJECTPANEL_FILTERS_COMBO));

					_filters = stringSplit(tfilters, TEXT(";"), true);

					// strange behaviour of stringSplit - it returns an array with a single empty string if it gets an empty string.
					if (_filters.size() == 1 && _filters[0].empty())
						_filters.clear();

					// check for nonsense combinations with *.* (which removes all filters)
					for (size_t i=0; i<_filters.size(); ++i)
					{
						if (_filters[i] == TEXT("*.*"))
						{
							_filters.clear();
							break;
						}
					}

					NppParameters *nppParams = NppParameters::getInstance();
					FindHistory & findHistory = nppParams->getFindHistory();
					saveComboHistory(IDD_PROJECTPANEL_FILTERS_COMBO, findHistory._nbMaxFindHistoryFilter, findHistory._findHistoryFilters);

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

void FilterDlg::addText2Combo(const TCHAR * txt2add, HWND hCombo)
{
	if (!hCombo) return;
	if (!lstrcmp(txt2add, TEXT(""))) return;

	int i = 0;

	i = ::SendMessage(hCombo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(txt2add));
	if (i != CB_ERR) // found
	{
		::SendMessage(hCombo, CB_DELETESTRING, i, 0);
	}

	i = ::SendMessage(hCombo, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(txt2add));

	::SendMessage(hCombo, CB_SETCURSEL, i, 0);
}

generic_string FilterDlg::getTextFromCombo(HWND hCombo, bool) const
{
	TCHAR str[PROJECTPANEL_FILTERS_MAXLENGTH];
	::SendMessage(hCombo, WM_GETTEXT, PROJECTPANEL_FILTERS_MAXLENGTH - 1, reinterpret_cast<LPARAM>(str));
	return generic_string(str);
}



void FilterDlg::fillComboHistory(int id, const std::vector<generic_string> & strings)
{
	HWND hCombo = ::GetDlgItem(_hSelf, id);

	for (std::vector<generic_string>::const_reverse_iterator i = strings.rbegin(); i != strings.rend(); ++i)
	{
		addText2Combo(i->c_str(), hCombo);
	}
	::SendMessage(hCombo, CB_SETCURSEL, 0, 0); // select first item
}

int FilterDlg::saveComboHistory(int id, int maxcount, std::vector<generic_string> & strings)
{
	TCHAR text[PROJECTPANEL_FILTERS_MAXLENGTH];
	HWND hCombo = ::GetDlgItem(_hSelf, id);
	int count = ::SendMessage(hCombo, CB_GETCOUNT, 0, 0);
	count = min(count, maxcount);

	if (count == CB_ERR) return 0;

	if (count)
		strings.clear();

	for (int i = 0; i < count; ++i)
	{
		::SendMessage(hCombo, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(text));
		strings.emplace_back(generic_string(text));
	}
	return count;
}


int FilterDlg::doDialog(bool isRTL /*= false*/)
{
	if (isRTL)
	{
		DLGTEMPLATE *pMyDlgTemplate = NULL;
		HGLOBAL hMyDlgTemplate = makeRTLResource(IDD_PROJECTPANEL_FILTERDIALOG, &pMyDlgTemplate);
		int result = ::DialogBoxIndirectParam(_hInst, pMyDlgTemplate, _hParent,  dlgProc, reinterpret_cast<LPARAM>(this));
		::GlobalFree(hMyDlgTemplate);
		return result;
	}


	return ::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_PROJECTPANEL_FILTERDIALOG), _hParent,  dlgProc, reinterpret_cast<LPARAM>(this));

}

ProjectPanel::TreeUpdaterDirectory::TreeUpdaterDirectory(ProjectPanel* projectPanel, HTREEITEM hItem) 
	: Directory()
	, _projectPanel(projectPanel)
	, _treeView(&projectPanel->getTreeView())
	, _hItem(hItem)
{
	TCHAR textBuffer[MAX_PATH];
	TVITEM tvItem;
	tvItem.mask = TVIF_TEXT | TVIF_PARAM;
	tvItem.pszText = textBuffer;
	tvItem.cchTextMax = MAX_PATH;

	tvItem.hItem = hItem;
	SendMessage(_treeView->getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
	ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);
	assert (data.isBaseDirectory() || data.isDirectory());
	if (!data.isBaseDirectory() && !data.isDirectory())
		return;
	_exists = PathFileExists(data._filePath.c_str()) != 0;
	if (_exists)
		_path = data._filePath;

    for (HTREEITEM hItemNode = _treeView->getChildFrom(hItem);
		hItemNode != NULL;
		hItemNode = _treeView->getNextSibling(hItemNode))
	{
		tvItem.hItem = hItemNode;
		SendMessage(_treeView->getHSelf(), TVM_GETITEM, 0,reinterpret_cast<LPARAM>(&tvItem));
		ProjectPanelData& data = *reinterpret_cast<ProjectPanelData*>(tvItem.lParam);

		generic_string filename(data._filePath);
		const size_t lastSlashIdx = filename.find_last_of(TEXT("\\/"));
		if (std::string::npos != lastSlashIdx)
			filename.erase(0, lastSlashIdx + 1);

		if (data.isDirectoryFile())
		{
			_files.insert(filename);
			_fileMap[filename] = hItemNode;
		}
		else if (data.isBaseDirectory() || data.isDirectory())
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


void ProjectPanel::TreeUpdaterDirectory::onDirAdded(const generic_string& name)
{
	_projectPanel->addDirectory(_hItem, name.c_str(), false, (_path + TEXT("\\") + name).c_str() );

}

void ProjectPanel::TreeUpdaterDirectory::onDirRemoved(const generic_string& name)
{
	_treeView->removeItem(_dirMap[name]);
}

void ProjectPanel::TreeUpdaterDirectory::onFileAdded(const generic_string& name)
{
	_treeView->addItem(name.c_str(), _hItem, IndexLeafDirFile, new ProjectPanelData(_projectPanel->_directoryWatcher, name.c_str(), (_path + TEXT("\\") + name).c_str(), nodeType_dirFile ));

}

void ProjectPanel::TreeUpdaterDirectory::onFileRemoved(const generic_string& name)
{
	_treeView->removeItem(_fileMap[name]);
}

int CALLBACK compareFunc(LPARAM lhs, LPARAM rhs, LPARAM)
{
	ProjectPanel::ProjectPanelData& dataL = *reinterpret_cast<ProjectPanel::ProjectPanelData*>(lhs);
	ProjectPanel::ProjectPanelData& dataR = *reinterpret_cast<ProjectPanel::ProjectPanelData*>(rhs);
	assert( (dataL.isDirectoryFile() || dataL.isDirectory()) && (dataR.isDirectoryFile() || dataR.isDirectory()));
	if( !((dataL.isDirectoryFile() || dataL.isDirectory()) && (dataR.isDirectoryFile() || dataR.isDirectory())))
		return 0;

	// one of the items is a folder, the other is not - folders are always on top
	if (dataL.isDirectory() != dataR.isDirectory())
		return dataL.isDirectory() ? -1 : 1;

	// both items are of the same kind.
	return lstrcmpi(dataL._name.c_str(),dataR._name.c_str());
}

void ProjectPanel::TreeUpdaterDirectory::onEndSynchronize(const Directory& other)
{
	other;
	_treeView->sort(_hItem, false, compareFunc);
}

