import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
 id:root
 required property var audio
 property bool compact:false
 property int page:0
 property string meterDevice:""
 readonly property var output:audio.outputs.find(d=>d.name===audio.defaultOutput)||({})
 readonly property var input:audio.inputs.find(d=>d.name===audio.defaultInput)||({})
 signal fullSettingsRequested()
 spacing:16
 Component.onCompleted:audio.setWatching(true)
 Component.onDestruction:{audio.monitorInput("",false);audio.stopRecording();audio.setWatching(false)}
 onPageChanged:{audio.monitorInput("",false);audio.stopRecording();meterDevice=""}
 component Title:Text {Layout.fillWidth:true;color:SettingsService.colors.text;font.pixelSize:17;font.bold:true;wrapMode:Text.Wrap}
 component Note:Text {Layout.fillWidth:true;color:SettingsService.colors.muted;font.pixelSize:12;wrapMode:Text.Wrap;textFormat:Text.PlainText}
 component Box:Rectangle {default property alias body:inside.data;Layout.fillWidth:true;implicitHeight:inside.implicitHeight+30;radius:16;color:SettingsService.colors.surface;border.color:SettingsService.colors.border;ColumnLayout {id:inside;anchors.fill:parent;anchors.margins:15;spacing:12}}
 component LevelControl:RowLayout {
  id:control;required property var entry;property bool input:false
  Layout.fillWidth:true
  Slider {id:slider;Layout.fillWidth:true;from:0;to:100;stepSize:1;enabled:!!control.entry.name
   Binding {target:slider;property:"value";value:control.entry.volume||0;when:!slider.pressed;restoreMode:Binding.RestoreNone}
   property bool dirty:false
   onMoved:{if(pressed)dirty=true;else root.audio.volume(control.entry.name,Math.round(value),control.input)}
   onPressedChanged:if(!pressed&&dirty&&enabled){dirty=false;root.audio.volume(control.entry.name,Math.round(value),control.input)}
   Accessible.name:control.input?"Microphone gain":"Output volume"
  }
  Note {Layout.fillWidth:false;text:Math.round(slider.value)+"%"}
  Switch {text:"Mute";checked:!!control.entry.muted;enabled:!!control.entry.name;onToggled:root.audio.mute(control.entry.name,checked,control.input);palette.windowText:SettingsService.colors.text}
 }
 RowLayout {
  Title {text:root.compact?"Sound":"Sound & Audio";font.pixelSize:root.compact?19:23}
  BusyIndicator {running:root.audio.busy;visible:running;implicitWidth:24;implicitHeight:24}
  SovelButton {text:"Refresh";visible:!root.compact;onClicked:root.audio.refresh()}
 }
 RowLayout {visible:!root.compact;Layout.fillWidth:true
  Repeater {model:["Output","Input","Applications","Advanced"]
   SovelButton {required property string modelData;required property int index;text:modelData;primary:root.page===index;onClicked:root.page=index}
  }
 }
 Box {visible:root.compact
  Note {text:"Output"}
  SovelCombo {Layout.fillWidth:true;model:root.audio.outputs;textRole:"description";currentIndex:Math.max(0,root.audio.outputs.findIndex(d=>d.name===root.audio.defaultOutput));onActivated:root.audio.selectOutput(root.audio.outputs[currentIndex].name);enabled:count>0}
  LevelControl {entry:root.output}
  Note {text:"Microphone"}
  SovelCombo {Layout.fillWidth:true;model:root.audio.inputs;textRole:"description";currentIndex:Math.max(0,root.audio.inputs.findIndex(d=>d.name===root.audio.defaultInput));onActivated:root.audio.selectInput(root.audio.inputs[currentIndex].name);enabled:count>0}
  Switch {text:"Mute microphone";enabled:!!root.input.name;checked:!!root.input.muted;onToggled:root.audio.mute(root.input.name,checked,true);palette.windowText:SettingsService.colors.text}
  SovelButton {text:"Sound Settings…";onClicked:root.fullSettingsRequested()}
 }
 ColumnLayout {visible:!root.compact&&root.page===0;Layout.fillWidth:true;spacing:12
  Note {text:"Choose speakers, headphones, USB, Bluetooth or monitor audio. HDMI and DisplayPort outputs appear when their driver/profile exposes them."}
  Repeater {model:root.audio.outputs
   Box {required property var modelData
    RowLayout {Layout.fillWidth:true
     RadioButton {checked:modelData.default;onClicked:root.audio.selectOutput(modelData.name);Accessible.name:"Use "+modelData.description}
     ColumnLayout {Layout.fillWidth:true;Title {text:modelData.description;font.pixelSize:15;textFormat:Text.PlainText} Note {text:modelData.kind+(modelData.default?" · Default output":"")}}
    }
    LevelControl {entry:modelData}
    RowLayout {
     SovelButton {text:"Test left";onClicked:root.audio.testSpeaker(modelData.name,0)}
     SovelButton {text:"Test right";enabled:(modelData.channels||[]).length>=2;onClicked:root.audio.testSpeaker(modelData.name,1)}
     Note {text:modelData.sampleSpec||"Format supplied by the device"}
    }
    RowLayout {visible:(modelData.channels||[]).length===2;Layout.fillWidth:true
     Note {text:"Left";Layout.fillWidth:false}
     Slider {Layout.fillWidth:true;from:-1;to:1;value:modelData.balance||0;stepSize:0.05;property bool dirty:false;onMoved:{if(pressed)dirty=true;else root.audio.balance(modelData.name,value)} onPressedChanged:if(!pressed&&dirty){dirty=false;root.audio.balance(modelData.name,value)}Accessible.name:"Left/right balance"}
     Note {text:"Right";Layout.fillWidth:false}
    }
   }
  }
  Note {visible:root.audio.outputs.length===0;text:"No outputs are currently exposed. Look under Advanced for HDMI/DisplayPort or surround profiles, or reconnect the device."}
 }
 ColumnLayout {visible:!root.compact&&root.page===1;Layout.fillWidth:true;spacing:12
  Note {text:"Microphones are captured only while you enable the meter or run a five-second test. Test recordings stay on this computer."}
  Repeater {model:root.audio.inputs
   Box {required property var modelData
    RowLayout {RadioButton {checked:modelData.default;onClicked:root.audio.selectInput(modelData.name)} Title {text:modelData.description;font.pixelSize:15;textFormat:Text.PlainText}}
    Note {text:modelData.kind+(modelData.noiseFiltered?" · Filtered source":"")+" · "+modelData.sampleSpec}
    LevelControl {entry:modelData;input:true}
    RowLayout {
     Switch {text:"Live level";checked:root.meterDevice===modelData.name;onToggled:{root.meterDevice=checked?modelData.name:"";root.audio.monitorInput(modelData.name,checked)} palette.windowText:SettingsService.colors.text}
     ProgressBar {Layout.fillWidth:true;from:0;to:1;value:root.meterDevice===modelData.name||root.audio.recording?root.audio.inputLevel:0}
    }
    RowLayout {SovelButton {text:root.audio.recording?"Stop test":"Test microphone";onClicked:{if(root.audio.recording)root.audio.stopRecording();else root.audio.recordTest(modelData.name)}} SovelButton {text:"Play test";enabled:!!root.audio.lastRecording&&!root.audio.recording;onClicked:root.audio.playRecording()}}
   }
  }
  Note {visible:root.audio.inputs.length===0;text:"No microphone is currently exposed by the audio service."}
  SovelButton {text:"Noise suppression & effects…";onClicked:root.audio.openEqualizer()}
  Note {text:"Hardware microphone boost and noise filtering depend on the device. Input gain above controls the exposed source; EasyEffects can provide optional filtering."}
 }
 ColumnLayout {visible:!root.compact&&root.page===2;Layout.fillWidth:true;spacing:12
  Note {visible:root.audio.streams.length===0;text:"Play audio or open a microphone in an app to see its active stream here."}
  Repeater {model:root.audio.streams
   Box {required property var modelData
    Title {text:modelData.description+(modelData.input?" · Recording":" · Playback");font.pixelSize:15;textFormat:Text.PlainText}
    RowLayout {Layout.fillWidth:true
     Slider {Layout.fillWidth:true;from:0;to:100;stepSize:1;value:modelData.volume;property bool dirty:false;onMoved:{if(pressed)dirty=true;else root.audio.streamVolume(modelData.id,Math.round(value),modelData.input)} onPressedChanged:if(!pressed&&dirty){dirty=false;root.audio.streamVolume(modelData.id,Math.round(value),modelData.input)}}
     Switch {text:"Mute";checked:modelData.muted;onToggled:root.audio.streamMute(modelData.id,checked,modelData.input);palette.windowText:SettingsService.colors.text}
    }
    SovelCombo {Layout.fillWidth:true;model:modelData.input?root.audio.inputs:root.audio.outputs;textRole:"description";currentIndex:Math.max(0,model.findIndex(d=>d.id===modelData.targetId));onActivated:root.audio.moveStream(modelData.id,model[currentIndex].name,modelData.input)}
   }
  }
 }
 ColumnLayout {visible:!root.compact&&root.page===3;Layout.fillWidth:true;spacing:12
  Box {Title {text:"Audio engine"} Note {text:root.audio.tuningAvailable?"These explicit preferences affect all apps. Automatic lets PipeWire negotiate with devices; unsupported formats may still be converted by the audio service.":"Engine tuning is unavailable because this session does not expose PipeWire metadata."}
   Note {text:"Preferred sample rate"}
   SovelCombo {Layout.fillWidth:true;enabled:root.audio.tuningAvailable;model:["Automatic","44.1 kHz","48 kHz","96 kHz","192 kHz"];property var values:[0,44100,48000,96000,192000];currentIndex:Math.max(0,values.indexOf(root.audio.preferredRate));onActivated:root.audio.preferredRate=values[currentIndex]}
   Note {text:"Buffer / quantum"}
   SovelCombo {Layout.fillWidth:true;enabled:root.audio.tuningAvailable;model:["Automatic","128 frames","256 frames","512 frames","1024 frames","2048 frames"];property var values:[0,128,256,512,1024,2048];currentIndex:Math.max(0,values.indexOf(root.audio.preferredQuantum));onActivated:root.audio.preferredQuantum=values[currentIndex]}
  }
  Repeater {model:root.audio.cards
   Box {required property var modelData
    Title {text:modelData.description;font.pixelSize:15;textFormat:Text.PlainText}
    Note {text:"Device profile / channel configuration. Available HDMI, DisplayPort, stereo and surround modes are supplied by this device."}
    SovelCombo {Layout.fillWidth:true;model:modelData.profiles;textRole:"description";currentIndex:Math.max(0,model.findIndex(p=>p.name===modelData.activeProfile));onActivated:root.audio.profile(modelData.name,model[currentIndex].name)}
   }
  }
  SovelButton {text:"Equalizer & audio effects…";onClicked:root.audio.openEqualizer()}
 }
 Note {text:root.audio.status}
}
