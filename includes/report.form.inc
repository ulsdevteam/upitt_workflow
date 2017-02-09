<?php

function upitt_workflow_objects() {
  global $user;

  if ($user->uid == 1 || user_access(ISLANDORA_WORKFLOW_MANAGE_LOOKUPTABLES, $user)) {
    $options = array('attributes' => array('title' => 'Requires "Workflow Manage Lookup Tables" permission'));
    $asterisk = "<sup style='color:blue' title='Requires \"Workflow Manage Lookup Tables\" permission'>*</super>";
    $links = array(l('Workflow Admin', '/workflow/admin', $options),
                   l('Create a New Batch', '/workflow/batch/new', $options),
                   l('Manage Lookup Tables', '/workflow/admin/manage/collection', $options));
  }

  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  // if there is any selection of the filters, then the data must be filtered with a WHERE clause, else don't add that condition
  $where_clause = '';

  if (isset($_GET['batch_external_id_filter']) && $_GET['batch_external_id_filter'] <> '') {
    $where_clause .= "i.name like '%" . upitt_workflow_safe_qstring($link, $_GET['batch_external_id_filter']) . "%' ";
  }

  if (isset($_GET['type_filter']) && $_GET['type_filter'] <> '') {
    $where_clause .= ($where_clause ? ' AND ' : '') . "it.name = '" . upitt_workflow_safe_qstring($link, $_GET['type_filter']) . "' ";
  }
  if (isset($_GET['collection_filter']) && $_GET['collection_filter'] <> '') {
    $where_clause .= ($where_clause ? ' AND ' : '') . "cc.name = '" . upitt_workflow_safe_qstring($link, $_GET['collection_filter']) . "' ";
  }

  $where_clause = ($where_clause) ? "WHERE " . $where_clause : ' ';

  $query = 'SELECT count(do_id) `total` ' .
           'FROM `item` i ' .
           'JOIN item_type it ON (it.id = i.type_id) ' .
           'JOIN collection cc ON (cc.id = i.primary_collection_id) ' .
           $where_clause;

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  if (!$result) {
     echo(mysqli_error($link));
  }

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);

  $query = 'SELECT "" as `ingested?`, i.do_id, i.name, it.name `type`, ' . //XX po.username, cc.name `collection`, ' .
           '  (SELECT GROUP_CONCAT(description) FROM transaction ct JOIN item_current_status cics ON (cics.last_transaction_id = ct.id) WHERE cics.item_id = i.id ORDER BY ct.timestamp DESC) `last status`, ' .
           '  (SELECT ct.timestamp FROM `transaction` ct JOIN item_current_status cics ON (cics.last_transaction_id = ct.id) WHERE cics.item_id = i.id ORDER BY ct.timestamp DESC LIMIT 1) `status date` ' .
           'FROM `item` i ' .
           'JOIN item_type it ON (it.id = i.type_id) ' .
           'JOIN collection cc ON (cc.id = i.primary_collection_id) ' .
           $where_clause .
           'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;
dpm($query);
  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
  }

  $ob1_page = $page + 1;
  $count_rows_markup = number_format($count_rows) . " workflow objects " . (($ob1_page <> 0) ? " (page " . $ob1_page . " of " . round($count_rows / WF_OBJECTS_PER_PAGE + 0.9999) . ")" : "");
  $headings = $rows = array();

  while ($row = mysqli_fetch_assoc($result)) {
    if (count($headings) < 1) {
      $headings = array_keys($row);
    }
    if (array_key_exists('do_id', $row)) {
      $pid = 'pitt:' . $row['do_id'];
      if ($obj = islandora_object_load($pid)) {
        $row['ingested?'] = l($row['do_id'], '/islandora/object/' . $pid, array('attributes' => array('target' => '_blank')));
      } else {
        $row['ingested?'] = '';
      }
      $row['do_id'] = l($row['do_id'], '/workflow/object/' . $row['do_id']);
    }
    $rows[] = $row;
  }

  mysqli_free_result($result);
  mysqli_close($link);
  $pager = theme('pager', array('quantity',$count_rows));
  $table_markup = theme('upitt_workflow_report',
      array('table' => $pager . theme('table', array('rows'=>$rows,'header'=>$headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'count_rows' => $count_rows_markup,
            'links' => $links,
            'breadcrumb' => '',
            'post_to' => '/workflow',
            'collection_filter_choices' => upitt_workflow_get_collection_choices(),
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);
  return $form;
}

function upitt_workflow_objectreport_form($do_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $status = upitt_workflow_get_status($do_id);

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $detail_query = 'SELECT b.batch_external_id `name`, b.batch_description `title`, b.mapto_collections `collection/s`, b.batch_id `batch_id` ' .
                  'FROM `item` i ' .
                  'JOIN batch_item bi ON (bi.item_id = i.id) ' .
                  'JOIN batch b ON (bi.batch_id = b.batch_id) ' .
                  'WHERE i.do_id = \'' . upitt_workflow_safe_qstring($link, $do_id) . '\'';

  $result = mysqli_query($link, $detail_query);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $detail_query;
    die($message);
  }
  $collections = upitt_workflow_lookup_collection_names();

  $pid = 'pitt:' . $do_id;
  $islandora_object = islandora_object_load($pid);
  
  $title = "Workflow object | " . (is_object($islandora_object) ? 'Islandora: "' . l($islandora_object->label, '/islandora/object/' . $pid) . '"' : $do_id);
  $files_headings = $status_headings = $files_rows = $required_workflow_actions = $status_rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['batch_id']) && isset($row['name'])) {
      $row['batch'] = l($row['name'], '/workflow/batch/' . $row['batch_id']) . '<br />';
      unset($row['batch_id']);
    }
    else {
      $row['batch'] = 'n/a';
    }
    if (isset($row['collection/s'])) {
      // this has to lookup the collection names from the array that is loaded before this loop.
      $row['collection/s'] = upitt_workflow_collection_id_map_names($row['collection/s'], $collections);
    }
    $details = theme('workflow_item_details', array('details' => $row));
  }
  if (!$details) {
    drupal_set_message('Workflow object "' . $do_id . '" not found.  <a href="javascript:history.go(-1);">Return to previous page.</a>');
    drupal_goto(variable_get('site_404', ''));
  }
  $links = array();

  // if there is any selection of the filters, then the data must be filtered with a WHERE clause, else don't add that condition
  $filters_use = $filters_mimetype = '';

  if (isset($_GET['use_filter']) && $_GET['use_filter'] <> '') {
    $filters_use = upitt_workflow_filters_clause($_GET['use_filter']);
  }
  if (isset($_GET['mimetype_filter']) && $_GET['mimetype_filter'] <> '') {
    $filters_mimetype = upitt_workflow_filters_clause($_GET['mimetype_filter']);
  }

  // allow to filter on cif.mime_type and cif.use
  // Files table --
  $files_query = 'SELECT f.name `name`, f.path `path`, f.mime_type `mime_type`, f.use `use`, f.timestamp ' .
                 'FROM item_file f ' .
                 'LEFT JOIN item i ON (f.item_id=i.id) ' .
                 'WHERE i.do_id=\'' . upitt_workflow_safe_qstring($link, $do_id) . '\'' .
                 (($filters_use) ? ' AND i.use IN ' . $filters_use : '') .
                 (($filters_mimetype) ? ' AND i.mime_type IN ' . $filters_mimetype : '') .
                 ' ' .
                 'ORDER BY f.name';

  $result = mysqli_query($link, $files_query);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $files_query;
    die($message);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($files_headings) < 1) {
      $files_headings = array_keys($row);
    }
    if (array_key_exists('do_id', $row)) {
      $pid = 'pitt:' . $row['do_id'];
      if ($obj = islandora_object_load($pid)) {
        $row['do_id'] = l($row['do_id'], '/islandora/object/' . $pid, array('attributes' => array('target' => '_blank')));
      } else {
        $row['do_id'] = l($row['do_id'], '/workflow/object/' . $row['do_id']);
      }
    }
    $files_rows[] = $row;
  }

  $workflow_sequence_name = '';
  // Get the required workflow sequence steps from this object's batch.  It will be used in conjunction with the status_query
  // below in order to display which items remain to be performed.
  $workflow_sequence_actions = 'SELECT a.name, a.description, wsa.`order`, ws.name `workflow_sequence_name` ' .
                               'FROM item i ' . 
                               'JOIN batch_item bi ON (bi.item_id = i.id) ' .
                               'JOIN batch b ON (b.batch_id = bi.batch_id) ' . 
                               'JOIN workflow_sequence ws ON (ws.id = b.batch_sequence_id) ' . 
                               'JOIN workflow_sequence_actions wsa ON (wsa.workflow_sequence_id = ws.id) ' .
                               'JOIN `action` a ON (a.id = wsa.action_id) ' .
                               'WHERE i.do_id = \'' . upitt_workflow_safe_qstring($link, $do_id) . '\' ' .
                               'ORDER BY wsa.`order`';
  $result = mysqli_query($link, $workflow_sequence_actions);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $workflow_sequence_actions;
    die($message);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    $required_workflow_actions[] = $row;
    $workflow_sequence_name = $row['workflow_sequence_name'];
  }
  

  // simple status ~ transactions table
  $status_query = 'SELECT t.description, t.`timestamp` `time` '.
                  'FROM item i ' .
                  'JOIN transaction t ON (t.item_id = i.id) ' .
//                  'JOIN collection co ON (co.id = i.primary_collection_id) ' .
                  'WHERE i.do_id=\'' . upitt_workflow_safe_qstring($link,  $do_id) . '\' ' .
                  'ORDER BY i.do_id DESC, t.`timestamp` ASC';

  $result = mysqli_query($link, $status_query);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $status_query;
    die($message);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($status_headings) < 1) {
      $status_headings = array_keys($row);
      $status_headings[] = 'when';
    }
    $row['when'] = upitt_workflow_timeago_from_timestamp($row['time']);
    $status_rows[] = $row;
  }
  mysqli_close($link);

  $status_table_problems = array();
  foreach ($required_workflow_actions as $required_workflow_action) {
    $completed_action_name = $required_workflow_action['name'] . ' completed';
    $completed_action_found = FALSE;
    foreach ($status_rows as  $k => $status_row) {
      if ($status_row['description'] == $completed_action_name) {
        $status_rows[$k]['description'] = '<span style="color:green">' . $status_rows[$k]['description'] . '</i>';
        $completed_action_found = TRUE;
      }
    }
    if (!$completed_action_found) {
      $status_table_problems[] = $required_workflow_action['name'] . ' not completed';
    }
  }
  foreach ($status_table_problems as $problem) {
    $status_rows[] = array('description' => '<span style="color:red">' . str_replace(" not completed", "", $problem) . '</span>', 'time' => '', 'when' => '');
  }

  $links[] = l('Edit METS', '/workflow/object/' . $do_id . '/METS/edit');
  $links[] = l('Islandora Ingest', '/workflow/object/' . $do_id . '/islandora_ingest');

  $table_markup = theme('upitt_workflow_report_with_detail_table',
      array('details' => $details,
            'workflow_sequence_name' => $workflow_sequence_name,
            'status_table' => theme('table', array('rows'=>$status_rows,'header'=>$status_headings)),
//  ((count($status_table_problems) > 0) ? '<div class="messages error">' . implode('</div><div class="messages error">', $status_table_problems) . '</div>' : '') . theme('table', array('rows'=>$status_rows,'header'=>$status_headings)),
            'files_table' => theme('table', array('rows'=>$files_rows,'header'=>$files_headings)),
            'title' => $title,
            'breadcrumb' => upitt_workflow_get_breadcrumb_path(),
            'links' => $links,
            'post_to' => '/workflow/object/' . $do_id,
           )
    );

  $content = array(
    'table_markup' => array('#markup' => $table_markup),
  );
  return $content;
}

function upitt_workflow_editMETS_form(array $form, array &$form_state, $do_id) {
  dpm($form_state);
  $xml_editor_markup = '<div id="xonomy_editor"></div>' . 
         '<button type="button" onclick="harvest();">Update METS file</button>';
  $form = array(
      'xmlfile' => array(
        '#attributes' => array(
          'id' => 'xmlfile',
          'style' => 'display:none',
        ),
        '#type' => 'textarea',
        '#prefix' => $xml_editor_markup,
        '#default_value' => (isset($form_state['input']['xmlfile']) ? htmlspecialchars($form_state['input']['xmlfile']) : ''),
      ),
      'submit' => array(
        '#type' => 'submit',
        '#value' => t('Save configuration'),
      ),
  );
  return $form;
}

function upitt_workflow_editMETS_form_submit(array $form, array &$form_state) {
//  dpm($form_state);
//  dpm($form);
}

function upitt_workflow_xonomy_editor_javascript_call_script($xml, $docSpec, $editor_id = 'xonomy_editor') {  
  return 'function start_xonomy_editor() {
    var docSpec=' . $docSpec . '
    var xml="' . $xml . '";
    var editor=document.getElementById("' . $editor_id . '");
    Xonomy.render(xml, editor, docSpec);
  }
  function harvest() {
    var xml=Xonomy.harvest();
    //do something with xml...
    $(\'#xmlfile\').text(xml);
    return false;
  }';
}

function upitt_workflow_docSpec() {
  return '{
	onchange: function(){
		//console.log("Ah been chaaanged!");
	},
	validate: function(jsElement){
		if(typeof(jsElement)=="string") jsElement=Xonomy.xml2js(jsElement);
		var valid=true;
		var elementSpec=this.elements[jsElement.name];
		if(elementSpec.validate) {
			elementSpec.validate(jsElement); //validate the element
		}
		for(var iAttribute=0; iAttribute<jsElement.attributes.length; iAttribute++) {
			var jsAttribute=jsElement.attributes[iAttribute];
			var attributeSpec=elementSpec.attributes[jsAttribute.name];
			if(attributeSpec.validate) {
				if(!attributeSpec.validate(jsAttribute)) valid=false; //validate the attribute
			}
		}
		for(var iChild=0; iChild<jsElement.children.length; iChild++) {
			if(jsElement.children[iChild].type=="element") {
				var jsChild=jsElement.children[iChild];
				if(!this.validate(jsChild)) valid=false; //recurse to the child element
			}
		}
		return valid;
	},
	elements: { 
		"mets": {
			menu: [],
			collapsed: function(jsElement){return false}
		},
                "mets:fileSec": {
                        backgroundColour: "#d6fad6",
                        menu: [],
                        collapsed: function(jsElement){return false},
                },
		"mets:fileGrp": {
			backgroundColour: "#e9e9e9",
			menu: [{
				caption: "New <mets:file>",
				action: Xonomy.newElementChild,
				actionParameter: "<mets:file ID=\'\' MIMETYPE=\'\' SEQ=\'\'/>",
				hideIf: function(jsElement){return false}
			}],
                        attributes: {
                                "TYPE": {
                                        asker: Xonomy.askString,
                                        askerParameter: {},
                                        menu: [],
                                        validate: function(jsAttribute) {
                                                if($.trim(jsAttribute.value)=="") {
                                                        Xonomy.warnings.push({htmlID: jsAttribute.htmlID, text: "The @TYPE attribute should not be empty."});
                                                        return false;
                                                }
                                                return true;
                                        },
                                },
                        },
			collapsed: function(jsElement){return false},
		},
		"mets:file": {
			//isReadOnly: function(jsMe){return true},
			//isInvisible: function(jsMe){return true},
			//displayName: "člověk",
			canDropTo: ["mets:fileGrp"],
			localDropOnly: true, // false,
			menu: [
				{	caption: "New <person> before this",
					action: Xonomy.newElementBefore,
					actionParameter: "<person name=\'\' sex=\'\'/>",
					hideIf: function(jsElement){return false}
				},
				{	caption: "New <person> after this",
					action: Xonomy.newElementAfter,
					actionParameter: "<person name=\'\' sex=\'\'/>",
					hideIf: function(jsElement){return false}
				},
				{	caption: "Add @age",
					action: Xonomy.newAttribute,
					actionParameter: {name: "age", value: ""},
					hideIf: function(jsElement){return jsElement.hasAttribute("age")}
				},
				{	caption: "Delete",
					action: Xonomy.deleteElement,
					actionParameter: null,
					hideIf: function(jsElement){return false}
				}
			],
			attributes: {
				"name": {
					asker: Xonomy.askString,
					askerParameter: {},
					menu: [],
					validate: function(jsAttribute) {
						if($.trim(jsAttribute.value)=="") {
							Xonomy.warnings.push({htmlID: jsAttribute.htmlID, text: "The @name attribute should not be empty."});
							return false;
						}
						return true;
					},
				},
				"sex": {
					shy: false,
					asker: Xonomy.askPicklist,
					askerParameter: [
						{value: "m", xdisplayValue: "muž", caption: "male"},
						{value: "f", xdisplayValue: "žen", caption: "female"},
						{value: "x", xdisplayValue: "jin", caption: "it\'s complicated"}
					],
					caption: function(jsMe){
						if(jsMe.value=="m") return "male";
						if(jsMe.value=="f") return "female";
						if(jsMe.value=="x") return "it\'s complicated";
					},
					// displayName: "pohlaví",
					// displayValue: function(jsMe){
						// if(jsMe.value=="m") return "muž";
						// if(jsMe.value=="f") return "žen";
						// if(jsMe.value=="x") return "jin";
						// return jsMe.value;
					// },
					menu: [],
					validate: function(jsAttribute) {
						if($.trim(jsAttribute.value)=="") {
							Xonomy.warnings.push({htmlID: jsAttribute.htmlID, text: "The @sex attribute should not be empty."});
							return false;
						}
						return true;
					},
				},
				"age": {
					shy: false,
					asker: Xonomy.askString,
					askerParameter: null,
					menu: [{
						caption: "Delete",
						action: Xonomy.deleteAttribute,
						actionParameter: null,
						hideIf: function(jsAttribute){return false}
					}],
					validate: function(jsAttribute) {
						if(!/^[0-9]+$/g.test(jsAttribute.value)) {
							Xonomy.warnings.push({htmlID: jsAttribute.htmlID, text: "The @age attribute should be a whole number greater than zero."});
							return false;
						}
						return true;
					},
				},
			},
			collapsed: function(jsElement){return false}
		},
                "mets:structMap": {
                        backgroundColour: "#ffd6d6",
                        menu: [{
                                caption: "New <div>",
                                action: Xonomy.newElementChild,
                                actionParameter: "<person name=\'\' sex=\'\'/>",
                                hideIf: function(jsElement){return false}
                        }],
                        collapsed: function(jsElement){return false},
                },
                "mets:div": {
                        //isReadOnly: function(jsMe){return true},
                        //isInvisible: function(jsMe){return true},
                        //displayName: "člověk",
                        canDropTo: ["mets:structMap"],
                        localDropOnly: true, // false,
                        menu: [
                                {       caption: "New <div> before this",
                                        action: Xonomy.newElementBefore,
                                        actionParameter: "<mets:div TYPE=\'page\' LABEL=\'unum\'/>",
                                        hideIf: function(jsElement){return false}
                                },
                                {       caption: "New <div> after this",
                                        action: Xonomy.newElementAfter,
                                        actionParameter: "<mets:div TYPE=\'page\' LABEL=\'unum\'/>",
                                        hideIf: function(jsElement){return false}
                                },
                                {       caption: "Add @div",
                                        action: Xonomy.newAttribute,
                                        actionParameter: {name: "div", value: ""},
                                        hideIf: function(jsElement){return jsElement.hasAttribute("div")}
                                },
                                {       caption: "Delete",
                                        action: Xonomy.deleteElement,
                                        actionParameter: null,
                                        hideIf: function(jsElement){return false}
                                }
                        ],
                        attributes: {
                                "FILEID": {
                                        asker: Xonomy.askString,
                                        askerParameter: {},
                                        menu: [],
                                        validate: function(jsAttribute) {
                                                if($.trim(jsAttribute.value)=="") {
                                                        Xonomy.warnings.push({htmlID: jsAttribute.htmlID, text: "The @name attribute should not be empty."});
                                                        return false;
                                                }
                                                return true;
                                        },
                                },
			},
		}

	}
}';
}

function upitt_workflow_generate_METS_xml($do_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  // IF the object already has an item file record that has a METS file, then use this... else generate a METS from the other MASTER tif files related to the object
  // PENDING connection to the file server so that the file can be obtained from there (with paths like: /usr/local/dlxs/repository/p/pitttext/31735066873732/31735066873732.mets.xml)

  $prefix = "<mets xmlns='http://www.loc.gov/METS/' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xmlns:mets='http://www.loc.gov/METS/' xmlns:mods='http://www.loc.gov/MODS' xmlns:xlink='http://www.w3.org/1999/xlink' xsi:schemaLocation='http://www.loc.gov/METS/ http://www.loc.gov/standards/mets/mets.xsd'><mets:fileSec><mets:fileGrp USE='master'>";
  $mid_point = "</mets:fileGrp></mets:fileSec><mets:structMap TYPE='mixed'><mets:div TYPE='volume'>";
  $suffix = "</mets:div></mets:structMap></mets>";

  $files = upitt_workflow_get_item_files($do_id);
  $mets_files = $struct_maps = array();

  $seq = 0;
  foreach ($files as $file) {
    if ($file['mime_type'] == 'IMAGE_TIFF') {
      $seq_padded = sprintf('%04s', $seq);
      @list($fid, $junk) = explode(".", $file['name']);
      $fid = 'fid' . $fid;
      
      $mets_files[] = "<mets:file ID='" . $fid . "' MIMETYPE='image/tiff' SEQ='" . $seq_padded . "'><mets:FLocat xlink:href='" . $file['name'] . "' LOCTYPE='URL'/></mets:file>";
      $struct_maps[] = "<mets:div TYPE='page' LABEL='unum'><mets:fptr FILEID='" . $fid . "'/></mets:div>";

      $seq++;
    }
  }

  $xml_string = $prefix .
                implode('', $mets_files) . 
                $mid_point . 
                implode('', $struct_maps) .
                $suffix;

  return $xml_string;
}
