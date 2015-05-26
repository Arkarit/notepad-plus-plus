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


#ifndef PROJECTPANEL_H
#define  PROJECTPANEL_H

//#include <windows.h>
#ifndef DOCKINGDLGINTERFACE_H
#include "DockingDlgInterface.h"
#endif //DOCKINGDLGINTERFACE_H

#include "TreeView.h"
#include "ProjectPanel_rc.h"

#define PM_PROJECTPANELTITLE     TEXT("Project")
#define PM_WORKSPACEROOTNAME     TEXT("Workspace")
#define PM_NEWFOLDERNAME         TEXT("Folder Name")
#define PM_NEWPROJECTNAME        TEXT("Project Name")

#define PM_NEWWORKSPACE            TEXT("New Workspace")
#define PM_OPENWORKSPACE           TEXT("Open Workspace")
#define PM_RELOADWORKSPACE         TEXT("Reload Workspace")
#define PM_SAVEWORKSPACE           TEXT("Save")
#define PM_SAVEASWORKSPACE         TEXT("Save As...")
#define PM_SAVEACOPYASWORKSPACE    TEXT("Save a Copy As...")
#define PM_NEWPROJECTWORKSPACE     TEXT("Add New Project")

#define PM_EDITRENAME              TEXT("Rename")
#define PM_EDITNEWFOLDER           TEXT("Add Folder")
#define PM_EDITADDFILES            TEXT("Add Files...")
#define PM_EDITADDFILESRECUSIVELY  TEXT("Add Files from Directory...")
#define PM_EDITADDFOLDERMONITOR    TEXT("Add Folder Monitor...")
#define PM_EDITREMOVE              TEXT("Remove\tDEL")
#define PM_EDITMODIFYFILE          TEXT("Modify File Path")

#define PM_WORKSPACEMENUENTRY      TEXT("Workspace")
#define PM_EDITMENUENTRY           TEXT("Edit")

#define PM_MOVEUPENTRY             TEXT("Move Up\tCtrl+Up")
#define PM_MOVEDOWNENTRY           TEXT("Move Down\tCtrl+Down")

enum NodeType {
	nodeType_root = 0, nodeType_project = 1, nodeType_folder = 2, nodeType_file = 3, nodeType_monitorFolderRoot = 4, nodeType_monitorFolder = 5, nodeType_monitorFile = 6,
};

class TiXmlNode;

class ProjectPanelFileData : public TreeViewData {
public:
	generic_string _filePath;
	NodeType _nodeType;
	DirectoryWatcher* _directoryWatcher;

	ProjectPanelFileData(HWND hWnd, const TCHAR* filePath, NodeType nodeType) : TreeViewData(), _nodeType(nodeType), _directoryWatcher(NULL) {
		if (filePath != NULL)
		{
			_filePath = generic_string(filePath);
			if (nodeType == nodeType_monitorFolderRoot)
				_directoryWatcher = new DirectoryWatcher(hWnd, _filePath);
		}
	}
	ProjectPanelFileData( const ProjectPanelFileData& other ) : _directoryWatcher(NULL) {
		_id = other._id;
		_filePath = other._filePath;
		_nodeType = other._nodeType;
		if( other._directoryWatcher )
			_directoryWatcher = new DirectoryWatcher(*other._directoryWatcher);
	}
	ProjectPanelFileData& operator= ( const ProjectPanelFileData& other ) {
		delete _directoryWatcher;
		_directoryWatcher = NULL;
		_id = other._id;
		_filePath = other._filePath;
		_nodeType = other._nodeType;
		if( other._directoryWatcher )
			_directoryWatcher = new DirectoryWatcher(*other._directoryWatcher);
		return *this;
	}

	virtual ~ProjectPanelFileData() {
		delete _directoryWatcher;
	}
	bool isRoot() const {
		return _nodeType == nodeType_root;
	}
	bool isProject() const {
		return _nodeType == nodeType_project;
	}
	bool isFile() const {
		return _nodeType == nodeType_file;
	}
	bool isFolder() const {
		return _nodeType == nodeType_folder;
	}
	bool isFileMonitor() const {
		return _nodeType == nodeType_monitorFile;
	}
	bool isFolderMonitor() const {
		return _nodeType == nodeType_monitorFolder;
	}
	bool isFolderMonitorRoot() const {
		return _nodeType == nodeType_monitorFolderRoot;
	}
	void startThreadIfNecessary(HTREEITEM item) {
		if (_directoryWatcher)
			_directoryWatcher->startThread(item);
	}
};


class ProjectPanel : public DockingDlgInterface, public TreeViewController {
public:
	ProjectPanel()
		: DockingDlgInterface(IDD_PROJECTPANEL)
		, _treeView(this)
		, _hToolbarMenu(NULL)
		, _hWorkSpaceMenu(NULL)
		, _hProjectMenu(NULL)
		, _hFolderMenu(NULL)
		, _hFileMenu(NULL)
		, _hFolderMonitorMenu(NULL) 
	{
	};

	virtual ~ProjectPanel() {
	}


	void init(HINSTANCE hInst, HWND hPere) {
		DockingDlgInterface::init(hInst, hPere);
	}

    virtual void display(bool toShow = true) const {
        DockingDlgInterface::display(toShow);
    };

    void setParent(HWND parent2set){
        _hParent = parent2set;
    };

	void newWorkSpace();
	bool openWorkSpace(const TCHAR *projectFileName);
	bool saveWorkSpace();
	bool saveWorkSpaceAs(bool saveCopyAs);
	void setWorkSpaceFilePath(const TCHAR *projectFileName){
		_workSpaceFilePath = projectFileName;
	};
	const TCHAR * getWorkSpaceFilePath() const {
		return _workSpaceFilePath.c_str();
	};
	bool isDirty() const {
		return _isDirty;
	};
	void checkIfNeedSave(const TCHAR *title);

	virtual void setBackgroundColor(COLORREF bgColour) {
		TreeView_SetBkColor(_treeView.getHSelf(), bgColour);
    };
	virtual void setForegroundColor(COLORREF fgColour) {
		TreeView_SetTextColor(_treeView.getHSelf(), fgColour);
    };

protected:
	TreeView _treeView;
	HIMAGELIST _hImaLst;
	HWND _hToolbarMenu;
	HMENU _hWorkSpaceMenu, _hProjectMenu, _hFolderMenu, _hFileMenu, _hFolderMonitorMenu;
	generic_string _workSpaceFilePath;
	generic_string _selDirOfFilesFromDirDlg;
	bool _isDirty;

	void initMenus();
	void destroyMenus();
	BOOL setImageList(int root_clean_id, int root_dirty_id, int project_id, int open_node_id, int closed_node_id, int leaf_id, int ivalid_leaf_id, int open_monitor_id, int closed_monitor_id, int invalid_monitor_id, int file_monitor_id);
	void addFiles(HTREEITEM hTreeItem);
	void addFilesFromDirectory(HTREEITEM hTreeItem, bool virtl);
	void recursiveAddFilesFrom(const TCHAR *folderPath, HTREEITEM hTreeItem, bool virtl);
	HTREEITEM addFolder(HTREEITEM hTreeItem, const TCHAR *folderName, bool virtl = false, bool root = false, const TCHAR *monitorPath = NULL);

	bool writeWorkSpace(TCHAR *projectFileName = NULL);
	generic_string getRelativePath(const generic_string & fn, const TCHAR *workSpaceFileName);
	void buildProjectXml(TiXmlNode *root, HTREEITEM hItem, const TCHAR* fn2write);
	NodeType getNodeType(HTREEITEM hItem) const;
	void setWorkSpaceDirty(bool isDirty);
	void popupMenuCmd(int cmdID);
	POINT getMenuDisplyPoint(int iButton);
	virtual BOOL CALLBACK ProjectPanel::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	bool buildTreeFrom(TiXmlNode *projectRoot, HTREEITEM hParentItem);
	void rebuildFolderMonitorTree(HTREEITEM hParentItem);
	void notified(LPNMHDR notification);
	void showContextMenu(int x, int y);
	generic_string getAbsoluteFilePath(const TCHAR * relativePath);
	void openSelectFile();
	HMENU getContextMenu(HTREEITEM hTreeItem) const;
#pragma warning( push )
#pragma warning( disable : 4100 )

	// TreeViewController
	virtual void onTreeItemAdded(bool afterClone, HTREEITEM hItem, TreeViewData* newData);
	virtual void onTreeItemRemoved(HTREEITEM hItem,TreeViewData* data) {}
	virtual void onTreeItemChanged(HTREEITEM hItem,TreeViewData* data);

	virtual void destroyDataInstance(HTREEITEM hItem, TreeViewData* data) {
		delete data;
	}
	virtual TreeViewData* cloneDataInstance(HTREEITEM hItem, TreeViewData* data) {
		return new ProjectPanelFileData( *((ProjectPanelFileData *) data) );
	}

	static ProjectPanelFileData* getInfo( TreeViewData* data ) {
		return (ProjectPanelFileData*) data;
	}

#pragma warning( pop )
};

class FileRelocalizerDlg : public StaticDialog
{
public :
	FileRelocalizerDlg() : StaticDialog() {};
	void init(HINSTANCE hInst, HWND parent){
		Window::init(hInst, parent);
	};

	int doDialog(const TCHAR *fn, bool isRTL = false);

    virtual void destroy() {
    };

	generic_string getFullFilePath() {
		return _fullFilePath;
	};

protected :
	virtual BOOL CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam);

private :
	generic_string _fullFilePath;

};

#endif // PROJECTPANEL_H
