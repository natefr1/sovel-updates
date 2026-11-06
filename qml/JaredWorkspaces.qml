import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
 id:root
 required property string page
 signal returnToChat()
 signal showFind()
 property int noteId:0
 property int projectId:0
 property int loadedNoteId:-1
 property bool savingNote:false
 readonly property var projects:Jared.workspaces.projects||[]
 function projectNames() {return ["No project"].concat(projects.map(project=>project.name))}
 function readNote(id) {noteId=id;loadedNoteId=-1;Jared.work("note-read",{id:id})}
 function newNote() {noteId=0;loadedNoteId=-1;noteTitle.clear();noteFolder.clear();noteBody.clear();noteProject.currentIndex=0;noteTitle.forceActiveFocus()}
 Connections {
  target:Jared
  function onChanged(){
   const note=Jared.workRecord
   if(root.savingNote&&!Jared.busy){
    root.savingNote=false
    if(note.savedNoteId){
     root.noteId=note.savedNoteId
     root.loadedNoteId=note.savedNoteId
    }
   }
   if(root.page!=="notes"||!root.noteId||note.id!==root.noteId||root.loadedNoteId===note.id||note.url)return
   root.loadedNoteId=note.id
   noteTitle.text=note.title
   noteFolder.text=note.folder
   noteBody.text=note.body
   noteProject.currentIndex=Math.max(0,root.projects.findIndex(project=>project.id===note.project)+1)
  }
 }
 StackLayout {
  anchors.fill:parent
  currentIndex:["notes","projects","research","find"].indexOf(root.page)
  RowLayout {
   Layout.fillWidth:true;Layout.fillHeight:true;spacing:12
   ColumnLayout {
    Layout.preferredWidth:Math.min(210,root.width*.35);Layout.fillHeight:true
    SovelButton {text:"New Note";enabled:!Jared.busy;onClicked:root.newNote()}
    TextField {id:noteSearch;Layout.fillWidth:true;placeholderText:"Search notes";selectByMouse:true}
    ListView {id:noteList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4
     model:(Jared.workspaces.notes||[]).filter(note=>[note.title,note.folder,note.excerpt].join(" ").toLowerCase().includes(noteSearch.text.trim().toLowerCase()))
     delegate:SovelButton {required property var modelData;width:ListView.view.width
      text:(modelData.folder?modelData.folder+" / ":"")+modelData.title
      enabled:!Jared.busy;onClicked:root.readNote(modelData.id)}
     ScrollBar.vertical:ScrollBar {}
    }
   }
   ColumnLayout {
    Layout.fillWidth:true;Layout.fillHeight:true
    TextField {id:noteTitle;Layout.fillWidth:true;placeholderText:"Note title";maximumLength:160;selectByMouse:true}
    RowLayout {
     TextField {id:noteFolder;Layout.fillWidth:true;placeholderText:"Folder (optional)";maximumLength:160;selectByMouse:true}
     ComboBox {id:noteProject;model:root.projectNames();Layout.maximumWidth:150;Accessible.name:"Note project"}
    }
    ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true
     TextArea {id:noteBody;placeholderText:"Write your note…";wrapMode:TextEdit.Wrap;selectByMouse:true;textFormat:TextEdit.PlainText}
    }
    RowLayout {
     SovelButton {text:"Save Note";primary:true;enabled:!Jared.busy&&!!noteTitle.text.trim()
      onClicked:{root.savingNote=true;Jared.work("note-save",{id:root.noteId,title:noteTitle.text,folder:noteFolder.text,body:noteBody.text,project:noteProject.currentIndex>0?root.projects[noteProject.currentIndex-1].id:0});if(!Jared.busy)root.savingNote=false}}
     SovelButton {text:"Summarize";enabled:!Jared.busy&&!!noteBody.text.trim();onClicked:{Jared.summarize(noteBody.text,noteTitle.text||"note");root.returnToChat()}}
    }
   }
  }
  ColumnLayout {
   Layout.fillWidth:true;Layout.fillHeight:true
   RowLayout {
    ComboBox {id:projectPicker;Layout.fillWidth:true;model:root.projects;textRole:"name"
     currentIndex:root.projects.findIndex(project=>project.id===root.projectId)
     onActivated:root.projectId=root.projects[currentIndex].id}
    SovelButton {text:"Add Project Folder";enabled:!Jared.busy;onClicked:projectFolder.open()}
   }
   SettingsLabel {Layout.fillWidth:true;text:projectPicker.currentIndex>=0?root.projects[projectPicker.currentIndex].path:"Choose a folder to start a project.";wrapMode:Text.WrapAnywhere}
   RowLayout {
    SovelButton {text:"Open in Files";enabled:!Jared.busy&&projectPicker.currentIndex>=0;onClicked:Jared.work("open",{path:root.projects[projectPicker.currentIndex].path})}
    SovelButton {text:"Recent Files";enabled:!Jared.busy&&root.projectId>0
     onClicked:{Jared.work("find",{query:"recent",project:root.projectId});root.showFind()}}
    SettingsLabel {text:taskList.count?((Jared.workspaces.tasks||[]).filter(task=>task.project===root.projectId&&task.done).length+" of "+taskList.count+" done"):"No tasks yet"}
   }
   ListView {id:taskList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true
    model:(Jared.workspaces.tasks||[]).filter(task=>task.project===root.projectId)
    delegate:CheckBox {required property var modelData;width:ListView.view.width;text:modelData.title;checked:!!modelData.done;enabled:!Jared.busy;onClicked:Jared.work("task-toggle",{id:modelData.id})}
    ScrollBar.vertical:ScrollBar{}
   }
   RowLayout {
    TextField {id:newTask;Layout.fillWidth:true;placeholderText:"New task";maximumLength:400;onAccepted:addTask.clicked()}
    SovelButton {id:addTask;text:"Add Task";enabled:!Jared.busy&&root.projectId>0&&!!newTask.text.trim()
     onClicked:{Jared.work("task-add",{title:newTask.text,project:root.projectId});newTask.clear()}}
   }
  }
  ColumnLayout {
   Layout.fillWidth:true;Layout.fillHeight:true
   RowLayout {
    TextField {id:webQuery;Layout.fillWidth:true;placeholderText:"Search the web";onAccepted:webButton.clicked()}
    SovelButton {id:webButton;text:"Search in Browser";enabled:!Jared.busy&&!!webQuery.text.trim();onClicked:Jared.work("web-search",{query:webQuery.text})}
   }
   RowLayout {
    TextField {id:articleUrl;Layout.fillWidth:true;placeholderText:"https:// article address";selectByMouse:true}
    SovelButton {text:"Read Article";enabled:!Jared.busy&&articleUrl.text.startsWith("https://");onClicked:Jared.work("article",{url:articleUrl.text})}
   }
   ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true
    TextArea {id:articleBody;text:Jared.articleText;placeholderText:"Article text or research notes";wrapMode:TextEdit.Wrap;selectByMouse:true;textFormat:TextEdit.PlainText}
   }
   RowLayout {
    ComboBox {id:researchProject;model:root.projectNames();Layout.maximumWidth:150;Accessible.name:"Research project"}
    SovelButton {text:"Save Research";enabled:!Jared.busy&&articleUrl.text.startsWith("https://")
     onClicked:Jared.work("research-save",{url:articleUrl.text,body:articleBody.text,project:researchProject.currentIndex>0?root.projects[researchProject.currentIndex-1].id:0})}
    SovelButton {text:"Summarize";enabled:!Jared.busy&&!!articleBody.text.trim();onClicked:{Jared.summarize(articleBody.text,articleUrl.text||"research");root.returnToChat()}}
    SovelButton {text:"Read Aloud";enabled:!!articleBody.text.trim();onClicked:Jared.speak(articleBody.text)}
   }
   ComboBox {id:savedResearch;Layout.fillWidth:true;model:Jared.workspaces.research||[];textRole:"title";currentIndex:-1
    displayText:currentIndex<0?"Saved research":currentText
    onActivated:{const entry=model[currentIndex];articleUrl.text=entry.url;Jared.work("research-read",{id:entry.id})}}
   Connections {target:Jared;function onChanged(){const record=Jared.workRecord;if(record.url&&record.id!==undefined&&record.url===articleUrl.text)articleBody.text=record.body}}
  }
  ColumnLayout {
   Layout.fillWidth:true;Layout.fillHeight:true
   SettingsLabel {Layout.fillWidth:true;text:"Searches Documents, Downloads and your project folders.";color:SettingsService.colors.muted}
   RowLayout {
    TextField {id:fileQuery;Layout.fillWidth:true;placeholderText:"Filename or extension";selectByMouse:true;onAccepted:findButton.clicked()}
    SovelButton {id:findButton;text:"Find Files";enabled:!Jared.busy&&!!fileQuery.text.trim();onClicked:Jared.work("find",{query:fileQuery.text})}
   }
   RowLayout {visible:displayChoice.count>1
    SettingsLabel {text:"Preview on"}
    SovelCombo {id:displayChoice;model:Jared.availableDisplays;Layout.fillWidth:true}
   }
   RowLayout {visible:root.projects.length>0
    SettingsLabel {text:"Tasks go to"}
    SovelCombo {Layout.fillWidth:true;model:root.projectNames()
     currentIndex:Math.max(0,root.projects.findIndex(project=>project.id===root.projectId)+1)
     onActivated:root.projectId=currentIndex>0?root.projects[currentIndex-1].id:0}
   }
   ListView {Layout.fillWidth:true;Layout.fillHeight:true;model:Jared.fileMatches;clip:true;spacing:6
    delegate:RowLayout {required property var modelData;width:ListView.view.width
     ColumnLayout {Layout.fillWidth:true
      SettingsLabel {Layout.fillWidth:true;text:modelData.name;font.bold:true}
      SettingsLabel {Layout.fillWidth:true;text:modelData.path;font.pixelSize:11;color:SettingsService.colors.muted;elide:Text.ElideMiddle}
      SettingsLabel {text:Qt.formatDateTime(new Date(modelData.modified*1000),Qt.locale().dateTimeFormat(Locale.ShortFormat));font.pixelSize:11;color:SettingsService.colors.muted}
     }
     SovelButton {text:"Open";enabled:!Jared.busy;onClicked:Jared.work("open",{path:modelData.path})}
     SovelButton {text:"Preview";visible:displayChoice.count>1&&!modelData.folder;enabled:!Jared.busy;onClicked:Jared.work("open-preview",{path:modelData.path,display:displayChoice.currentText})}
     SovelButton {text:"Review Tasks";visible:root.projectId>0&&!modelData.folder&&/\.(md|txt|rst)$/i.test(modelData.name)
      enabled:!Jared.busy&&!Jared.pendingAction.type
      onClicked:{Jared.work("task-extract",{path:modelData.path,project:root.projectId});root.returnToChat()}}
    }
    ScrollBar.vertical:ScrollBar {}
   }
  }
 }
 FolderDialog {id:projectFolder;title:"Choose a project folder"
  onAccepted:Jared.work("project-add",{path:decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//,""))})}
}
