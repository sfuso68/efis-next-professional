import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: 1
    property string editDisposition: "UNKNOWN"
    readonly property var profiles: controller.operationalProfiles || []
    readonly property var domains: controller.operationalDomains || []
    readonly property var fields: controller.operationalFields || []
    readonly property var summary: controller.operationalSummary || ({})
    readonly property var selected: controller.operationalSelectedField || ({})
    readonly property color cyan: "#28D9F4"
    readonly property color green: "#52DE36"
    readonly property color amber: "#FFC72B"
    readonly property color red: "#FF5050"
    readonly property color line: "#40535E"
    readonly property color panel: "#E00A1821"

    function syncEditor(field) {
        if (!field || field.key === undefined) {
            editDisposition = "UNKNOWN"
            valueInput.text = ""
            rationaleInput.text = ""
            sourceInput.text = ""
            revisionInput.text = ""
            pageInput.text = ""
            return
        }
        editDisposition = String(field.disposition || "UNKNOWN")
        valueInput.text = editDisposition !== "VALUE" || field.value === undefined ? "" :
            (typeof field.value === "object" ? JSON.stringify(field.value) : String(field.value))
        rationaleInput.text = String(field.rationale || "")
        sourceInput.text = String(field.source_document || "")
        revisionInput.text = String(field.source_revision || "")
        pageInput.text = String(field.source_page || "")
    }
    function chooseField(field) {
        controller.selectOperationalField(String(field.key))
    }
    onSelectedChanged: syncEditor(selected)
    Component.onCompleted: syncEditor(selected)
    function cycleProfile() {
        if (!profiles.length) return
        var current = String(controller.operationalSelectedProfile || "")
        var index = 0
        for (var i=0; i<profiles.length; ++i) if (String(profiles[i].profile_key) === current) index=i
        controller.selectOperationalProfile(String(profiles[(index+1)%profiles.length].profile_key))
    }

    Rectangle { anchors.fill:parent; color:"transparent"
        Rectangle { id:summaryBar; anchors { left:parent.left; right:parent.right; top:parent.top } height:58*root.uiScale; color:root.panel; border.color:root.line
            Text { x:10*root.uiScale; y:7*root.uiScale; text:"CONTROLLED AIRCRAFT PROFILE"; color:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
            Text { x:10*root.uiScale; y:25*root.uiScale; width:365*root.uiScale; elide:Text.ElideRight; text:String(controller.operationalSelectedProfile||"NO PROFILE")+"  ›"; color:"white"; font.pixelSize:11*root.uiScale; font.bold:true
                TapHandler { onTapped:root.cycleProfile() }
            }
            Row { anchors { right:parent.right; rightMargin:10*root.uiScale; verticalCenter:parent.verticalCenter } spacing:7*root.uiScale
                Repeater { model:[
                    {k:"FIELDS",v:Number(root.summary.total||0),c:root.cyan},
                    {k:"UNKNOWN",v:Number(root.summary.unresolved||0),c:root.amber},
                    {k:"SAFETY BLOCK",v:Number(root.summary.safety_blockers||0),c:root.red},
                    {k:"SIMULATED",v:Number(root.summary.simulated||0),c:root.red},
                    {k:"EDITED",v:Number(root.summary.edited||0),c:root.green}]
                    delegate:Rectangle { required property var modelData; width:69*root.uiScale; height:42*root.uiScale; radius:3*root.uiScale; color:"#102630"; border.color:modelData.c
                        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:4*root.uiScale } text:modelData.k; color:modelData.c; font.pixelSize:5.5*root.uiScale; font.bold:true }
                        Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:3*root.uiScale } text:modelData.v; color:"white"; font.pixelSize:14*root.uiScale; font.bold:true }
                    }
                }
            }
        }

        Rectangle { id:domainPane; anchors { left:parent.left; top:summaryBar.bottom; bottom:statusBar.top; topMargin:7*root.uiScale } width:154*root.uiScale; color:root.panel; border.color:root.line
            Text { x:9*root.uiScale; y:7*root.uiScale; text:"DOMAINS • 28"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
            ListView { anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:25*root.uiScale; margins:5*root.uiScale } clip:true; spacing:2*root.uiScale; model:root.domains
                delegate:Rectangle { required property var modelData; width:ListView.view.width; height:29*root.uiScale; color:String(controller.operationalSelectedDomain)===String(modelData.id)?"#174052":"transparent"; border.color:String(controller.operationalSelectedDomain)===String(modelData.id)?root.cyan:"transparent"
                    Text { x:7*root.uiScale; anchors.verticalCenter:parent.verticalCenter; width:112*root.uiScale; elide:Text.ElideRight; text:String(modelData.label||modelData.id).toUpperCase(); color:String(controller.operationalSelectedDomain)===String(modelData.id)?"white":"#AAB6BC"; font.pixelSize:6.5*root.uiScale; font.bold:true }
                    Text { anchors { right:parent.right; rightMargin:6*root.uiScale; verticalCenter:parent.verticalCenter } text:String(modelData.parameter_count||0); color:root.cyan; font.pixelSize:6*root.uiScale }
                    TapHandler { onTapped:controller.selectOperationalDomain(String(modelData.id)) }
                }
            }
        }

        Rectangle { id:listPane; anchors { left:domainPane.right; top:domainPane.top; bottom:domainPane.bottom; leftMargin:7*root.uiScale } width:292*root.uiScale; color:root.panel; border.color:root.line
            Rectangle { x:7*root.uiScale; y:7*root.uiScale; width:parent.width-14*root.uiScale; height:29*root.uiScale; radius:3*root.uiScale; color:"#102630"; border.color:root.line
                TextInput { id:searchInput; anchors { fill:parent; margins:6*root.uiScale } color:"white"; font.pixelSize:7*root.uiScale; clip:true; selectByMouse:true; text:""; onTextEdited:controller.filterOperationalFields(text) }
                Text { visible:!searchInput.text.length; x:6*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"SEARCH FIELD / IDENTIFIER"; color:"#70838C"; font.pixelSize:7*root.uiScale }
            }
            ListView { anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:43*root.uiScale; margins:6*root.uiScale } clip:true; spacing:3*root.uiScale; model:root.fields
                delegate:Rectangle { required property var modelData; required property int index; width:ListView.view.width; height:48*root.uiScale; color:String(root.selected.key)===String(modelData.key)?"#163847":(index%2?"#0B1922":"#10222C"); border.color:modelData.blocker?root.red:(modelData.unresolved?root.amber:"transparent")
                    Rectangle { width:4*root.uiScale; height:parent.height; color:modelData.blocker?root.red:(modelData.edited?root.green:"transparent") }
                    Text { x:9*root.uiScale; y:5*root.uiScale; width:parent.width-18*root.uiScale; elide:Text.ElideRight; text:String(modelData.label||""); color:"white"; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { x:9*root.uiScale; y:22*root.uiScale; width:parent.width-92*root.uiScale; elide:Text.ElideRight; text:String(modelData.display_value||"—")+(modelData.canonical_unit?" "+String(modelData.canonical_unit):""); color:modelData.unresolved?root.amber:root.cyan; font.pixelSize:7*root.uiScale }
                    Row { anchors { right:parent.right; rightMargin:6*root.uiScale; bottom:parent.bottom; bottomMargin:6*root.uiScale } spacing:4*root.uiScale
                        Text { text:modelData.effective_simulated?"SIM":""; color:root.red; font.pixelSize:5.5*root.uiScale; font.bold:true }
                        Text { text:String(modelData.criticality)==="SAFETY_CRITICAL"?"SAFETY":""; color:root.red; font.pixelSize:5.5*root.uiScale; font.bold:true }
                    }
                    TapHandler { onTapped:root.chooseField(modelData) }
                }
            }
        }

        Rectangle { id:editor; anchors { left:listPane.right; right:parent.right; top:domainPane.top; bottom:domainPane.bottom; leftMargin:7*root.uiScale } color:root.panel; border.color:root.selected.blocker?root.red:root.line
            Text { x:10*root.uiScale; y:8*root.uiScale; width:parent.width-20*root.uiScale; elide:Text.ElideRight; text:String(root.selected.label||"SELECT A FIELD"); color:"white"; font.pixelSize:10*root.uiScale; font.bold:true }
            Text { x:10*root.uiScale; y:27*root.uiScale; width:parent.width-20*root.uiScale; elide:Text.ElideMiddle; text:String(root.selected.key||""); color:root.cyan; font.pixelSize:6*root.uiScale }
            Text { x:10*root.uiScale; y:43*root.uiScale; text:String(root.selected.value_type||"—")+(root.selected.canonical_unit?" • "+String(root.selected.canonical_unit):"")+" • "+String(root.selected.requirement||""); color:"#AAB6BC"; font.pixelSize:6*root.uiScale }
            Rectangle { visible:root.selected.effective_simulated===true; anchors { right:parent.right; rightMargin:10*root.uiScale; top:parent.top; topMargin:8*root.uiScale } width:96*root.uiScale; height:25*root.uiScale; radius:2*root.uiScale; color:"#4A1117"; border.color:root.red
                Text { anchors.centerIn:parent; text:"SIMULATED"; color:"white"; font.pixelSize:6.5*root.uiScale; font.bold:true }
            }

            Grid { x:10*root.uiScale; y:62*root.uiScale; columns:3; spacing:4*root.uiScale
                Repeater { model:[{l:"VALUE",v:"VALUE"},{l:"UNKNOWN",v:"UNKNOWN"},{l:"INHERIT",v:"INHERIT"},{l:"N/A",v:"NOT_APPLICABLE"},{l:"NOT INST.",v:"NOT_INSTALLED"}]
                    delegate:Rectangle { required property var modelData; width:83*root.uiScale; height:25*root.uiScale; radius:2*root.uiScale; color:root.editDisposition===modelData.v?"#1A4857":"#102630"; border.color:root.editDisposition===modelData.v?root.cyan:root.line
                        Text { anchors.centerIn:parent; text:modelData.l; color:root.editDisposition===modelData.v?"white":"#9EADB4"; font.pixelSize:5.8*root.uiScale; font.bold:true }
                        TapHandler { onTapped:root.editDisposition=modelData.v }
                    }
                }
            }

            component FieldBox: Rectangle {
                property alias value: input.text
                property string hint: ""
                width:parent.width-20*root.uiScale; height:29*root.uiScale; radius:2*root.uiScale; color:"#0B1820"; border.color:root.line
                TextInput { id:input; anchors { fill:parent; margins:6*root.uiScale } color:"white"; font.pixelSize:6.5*root.uiScale; clip:true; selectByMouse:true }
                Text { visible:!input.text.length; x:6*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:parent.hint; color:"#647881"; font.pixelSize:6*root.uiScale }
            }
            FieldBox { id:valueInput; x:10*root.uiScale; y:121*root.uiScale; hint:"VALUE • JSON FOR TABLE/CURVE" }
            FieldBox { id:sourceInput; x:10*root.uiScale; y:154*root.uiScale; hint:"SOURCE DOCUMENT" }
            Row { x:10*root.uiScale; y:187*root.uiScale; spacing:5*root.uiScale
                FieldBox { id:revisionInput; width:125*root.uiScale; hint:"REVISION" }
                FieldBox { id:pageInput; width:125*root.uiScale; hint:"PAGE" }
            }
            FieldBox { id:rationaleInput; x:10*root.uiScale; y:220*root.uiScale; hint:"RATIONALE / APPLICABILITY" }

            Rectangle { x:10*root.uiScale; y:258*root.uiScale; width:161*root.uiScale; height:34*root.uiScale; radius:3*root.uiScale; color:"#154234"; border.color:root.green
                Text { anchors.centerIn:parent; text:"SAVE DRAFT FIELD"; color:"white"; font.pixelSize:7*root.uiScale; font.bold:true }
                TapHandler { onTapped:controller.stageOperationalField(root.editDisposition,valueInput.text,rationaleInput.text,sourceInput.text,revisionInput.text,pageInput.text) }
            }
            Rectangle { x:178*root.uiScale; y:258*root.uiScale; width:82*root.uiScale; height:34*root.uiScale; radius:3*root.uiScale; color:"#3A211B"; border.color:root.amber
                Text { anchors.centerIn:parent; horizontalAlignment:Text.AlignHCenter; text:"HOLD 1.2s\nCLEAR DRAFT"; color:root.amber; font.pixelSize:5.5*root.uiScale; font.bold:true }
                TapHandler { longPressThreshold:1.2; onLongPressed:controller.discardOperationalDraft() }
            }
            Text { x:10*root.uiScale; y:300*root.uiScale; width:parent.width-20*root.uiScale; wrapMode:Text.WordWrap; text:"Safety-critical VALUE fields require controlled document, revision and page before a future release. This screen cannot activate a profile."; color:root.amber; font.pixelSize:6*root.uiScale }
        }

        Rectangle { id:statusBar; anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:31*root.uiScale; color:"#301015"; border.color:root.red
            Text { x:9*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:root.summary.simulation_profile?"⛔  SIMULATION DATA • NOT FOR FLIGHT • RELEASE READY = NO":"⛔  DRAFT ONLY • RELEASE READY = NO"; color:root.red; font.pixelSize:7*root.uiScale; font.bold:true }
            Text { anchors { right:parent.right; rightMargin:9*root.uiScale; verticalCenter:parent.verticalCenter } width:420*root.uiScale; horizontalAlignment:Text.AlignRight; elide:Text.ElideLeft; text:String(controller.operationalCatalogStatus||"NOT LOADED"); color:root.amber; font.pixelSize:6.5*root.uiScale; font.bold:true }
        }
    }
}
