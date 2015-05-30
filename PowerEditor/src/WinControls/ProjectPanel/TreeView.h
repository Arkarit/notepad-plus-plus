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

#ifndef TREE_VIEW_H
#define TREE_VIEW_H

#include "window.h"
#include "DirectoryWatcher.h"
#include <set>

struct TreeStateNode {
	generic_string _label;
	GUID _id;
	bool _isExpanded;
	bool _isSelected;
	std::vector<TreeStateNode> _children;
};

// note that this is no real MVC implementation... in fact the view does a lot of work the controller should do instead.
// The previous implementation was based on a treeview only, and I did not want to write everything new. So the "controller" is instead more a listener and factory.

class TreeViewData {
protected:
	GUID _id;
public:
	TreeViewData() {
		CoCreateGuid(&_id);
	}
	virtual ~TreeViewData() {}

	virtual const GUID& getId() const {
		return _id;
	}

	virtual TreeViewData* clone() const = 0;

};

//ha I suggest to switch off C4100 (unreferenced formal parameter) globally, because it does more harm than good. Optional virtual methods become unreadable with this warning.
#pragma warning( push )
#pragma warning( disable : 4100 )

class TreeViewListener {
public:
	virtual void onTreeItemAdded(bool afterClone, HTREEITEM hItem, TreeViewData* newData) {}
	virtual void onTreeItemRemoved(HTREEITEM hItem,TreeViewData* data){}
	virtual void onTreeItemChanged(HTREEITEM hItem,TreeViewData* data){}
};

#pragma warning( pop )

class TreeView : public Window {
public:
	TreeView() : Window(), _isItemDragged(false), _listener(NULL) {};

	virtual ~TreeView() {};
	virtual void init(HINSTANCE hInst, HWND parent, int treeViewID);
	virtual void destroy();
	HTREEITEM addItem(const TCHAR *itemName, HTREEITEM hParentItem, int iImage, TreeViewData* data);
	HTREEITEM searchSubItemByName(const TCHAR *itemName, HTREEITEM hParentItem);
	void removeItem(HTREEITEM hTreeItem);
	void removeAllItems();
	void removeAllChildren(HTREEITEM hParent);

	HTREEITEM getChildFrom(HTREEITEM hTreeItem) const {
		return TreeView_GetChild(_hSelf, hTreeItem);
	};
	HTREEITEM getSelection() const {
		return TreeView_GetSelection(_hSelf);
	};
	bool selectItem(HTREEITEM hTreeItem2Select) const {
		return TreeView_SelectItem(_hSelf, hTreeItem2Select) == TRUE;
	};
	HTREEITEM getRoot() const {
		return TreeView_GetRoot(_hSelf);
	};
	HTREEITEM getParent(HTREEITEM hItem) const {
		return TreeView_GetParent(_hSelf, hItem);
	};
	HTREEITEM getNextSibling(HTREEITEM hItem) const {
		return TreeView_GetNextSibling(_hSelf, hItem);
	};
	HTREEITEM getPrevSibling(HTREEITEM hItem) const {
		return TreeView_GetPrevSibling(_hSelf, hItem);
	};
	
	void expand(HTREEITEM hItem) const {
		TreeView_Expand(_hSelf, hItem, TVE_EXPAND);
	};

	void fold(HTREEITEM hItem) const {
		TreeView_Expand(_hSelf, hItem, TVE_COLLAPSE);
	};

	void toggleExpandCollapse(HTREEITEM hItem) const {
		TreeView_Expand(_hSelf, hItem, TVE_TOGGLE);
	};

	bool isExpanded(HTREEITEM hItem) const {
		TVITEM tvItem;
		tvItem.mask = TVIF_PARAM | TVIF_STATE;
		tvItem.hItem = hItem;
		::SendMessage(_hSelf, TVM_GETITEM, 0, (LPARAM)&tvItem);

		return (tvItem.state & TVIS_EXPANDED) != 0;
	}

	bool isVisible(HTREEITEM hItem) const {
		for (hItem=getParent(hItem); hItem != NULL; hItem=getParent(hItem))
		{
			if( !isExpanded(hItem))
				return false;
		}
		return true;
	}

	void setItemImage(HTREEITEM hTreeItem, int iImage, int iSelectedImage);

	// Drag and Drop operations
	void beginDrag(NMTREEVIEW* tv);
	void dragItem(HWND parentHandle, int x, int y);
	bool isDragging() const {
		return _isItemDragged;
	};
	bool dropItem();
	void addCanNotDropInList(int val2set) {
		_canNotDropInList.push_back(val2set);
	};

	void addCanNotDragOutList(int val2set) {
		_canNotDragOutList.push_back(val2set);
	};

	bool moveDown(HTREEITEM itemToMove);
	bool moveUp(HTREEITEM itemToMove);
	bool swapTreeViewItem(HTREEITEM itemGoDown, HTREEITEM itemGoUp);
	bool restoreFoldingStateFrom(const TreeStateNode & treeState2Compare, HTREEITEM treeviewNode);
	bool retrieveFoldingStateTo(TreeStateNode & treeState2Construct, HTREEITEM treeviewNode);
	bool searchLeafAndBuildTree(TreeView & tree2Build, const generic_string & text2Search, int index2Search);
	void sort(HTREEITEM hTreeItem);

	bool itemValid(HTREEITEM item) {
		return _validHandles.find(item) != _validHandles.end();
	}

	TreeViewData* getData(HTREEITEM item);

	void setListener(TreeViewListener* listener)
	{
		_listener = listener;
	}

protected:
	TreeViewListener* _listener;
	std::set<HTREEITEM> _validHandles;

	WNDPROC _defaultProc;
	LRESULT runProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK staticProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
		return (((TreeView *)(::GetWindowLongPtr(hwnd, GWL_USERDATA)))->runProc(hwnd, Message, wParam, lParam));
	};
	void cleanSubEntries(HTREEITEM hTreeItem);
	void dupTree(HTREEITEM hTree2Dup, HTREEITEM hParentItem);
	bool searchLeafRecusivelyAndBuildTree(HTREEITEM tree2Build, const generic_string & text2Search, int index2Search, HTREEITEM tree2Search);

	void synchronizeTree(HTREEITEM parent);

	// Drag and Drop operations
	HTREEITEM _draggedItem;
	HIMAGELIST _draggedImageList;
	bool _isItemDragged;
	std::vector<int> _canNotDragOutList;
	std::vector<int> _canNotDropInList;
	bool canBeDropped(HTREEITEM draggedItem, HTREEITEM targetItem);
	void moveTreeViewItem(HTREEITEM draggedItem, HTREEITEM targetItem);
	bool isParent(HTREEITEM targetItem, HTREEITEM draggedItem);
	bool isDescendant(HTREEITEM targetItem, HTREEITEM draggedItem);
	bool canDragOut(HTREEITEM targetItem);
	bool canDropIn(HTREEITEM targetItem);
};


#endif // TREE_VIEW_H
