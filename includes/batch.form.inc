<?php

function upitt_workflow_new_batch_form(array $form, array &$form_state, $batch_id = NULL) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('module', 'islandora_solution_pack_collection', 'islandora_basic_collection');
  $batch_item_type_options = upitt_workflow_get_mysql_options('batch_item_type', 'Item Type', 'name', 'name', 'name', '0');
  $batch_property_owners = upitt_workflow_get_mysql_options('property_owner', 'Property Owner', 'property_owner_id', 'description', 'description', '0');
  $batch_workflow_sequences = upitt_workflow_get_mysql_options('workflow_sequence', 'Workflow Sequence', 'id', 'name', 'id', '0');


  $collections = upitt_workflow_get_solr_options('RELS_EXT_hasModel_uri_ms:info\:fedora\/islandora\:collectionCModel', 'PID', 'fgs_label_s');
  asort($collections);

  $read_only_att = array();
  $nid = '';
  if ($batch_id) {
    $batch = upitt_workflow_batch_load($batch_id);
    if (is_array($batch)) {
      $read_only_att = array('readonly' => 'readonly', 'disabled' => 'disabled');
      $nid = $batch['nid'];
    }
  }

  $form = array(
    'nid' => array(
      '#type' => 'hidden',
      '#default_value' => $nid,
    ),
    // Name: textfield
    'batch_external_id' => array(
      '#type' => 'textfield',
      '#title' => t('Name'),
      '#attributes' => $read_only_att,
      '#description' => t('<pre>batch_external_id</pre>'),
      '#default_value' => ($batch && isset($batch['batch_external_id']))? $batch['batch_external_id'] : '',
    ),

//      '#default_value' => ($batch && isset($batch['']))? $batch[''] : '',

    // Collection: select box
    //   THIS SELECTBOX WILL BE A PROBLEM SINCE WE MAP TO MORE THAN ONE COLLECTION
    'mapto_collections' => array(
      '#type' => 'select',
      '#description' => 'Hold down Ctrl to select multiple collections',
      '#title' => t('Collection'),
      '#multiple' => TRUE,
      '#options' => $collections,
      '#default_value' => ($batch && isset($batch['mapto_collections']))? explode(",", $batch['mapto_collections']) : '',
      '#size' => 10,
    ),
    // Type: select (get options from `batch_item_type`)
    'batch_item_type_id' => array(
      '#type' => 'select',
      '#title' => t('Type'),
      '#options' => $batch_item_type_options,
      '#default_value' => ($batch && isset($batch['content_type_id']))? $batch['content_type_id'] : '',
    ),
    // Property owner: select (get options from `property_owners`)
    'batch_property_owner_id' => array(
      '#type' => 'select',
      '#title' => t('Property Owner'),
      '#options' => $batch_property_owners,
      '#default_value' => ($batch && isset($batch['property_owner_id']))? $batch['property_owner_id'] : '',
    ),
    // Description: textarea
    'batch_description' => array(
      '#type' => 'textarea',
      '#title' => t('Description'),
      '#default_value' => ($batch && isset($batch['batch_description']))? $batch['batch_description'] : '',
    ),
    // Sequence: select (get options from `workflow_sequence`)
    'batch_sequence_id' => array(
      '#type' => 'select',
      '#title' => t('Sequence'),
      '#options' => $batch_workflow_sequences,
      '#default_value' => ($batch && isset($batch['sequence_id']))? $batch['sequence_id'] : '',
    ),
    // Active? checkbox
    'is_batch_active' => array(
      '#type' => 'checkbox',
      '#title' => t('Active?'),
      '#default_value' => ($batch && isset($batch['is_batch_active']))? $batch['is_batch_active'] : '',
    ),
    // Priority: textfield
    'batch_priority' => array(
      '#type' => 'textfield',
      '#title' => t('Priority'),
      '#default_value' => ($batch && isset($batch['batch_priority']))? $batch['batch_priority'] : '',
    ),
    // Source identifier: textfield
    'batch_source_identifier' => array(
      '#type' => 'textfield',
      '#title' => t('Source Identifier'),
      '#default_value' => ($batch && isset($batch['batch_source_identifier']))? $batch['batch_source_identifier'] : '',
    ),
    // Is a request? checkbox
    'is_batch_request' => array(
      '#type' => 'checkbox',
      '#title' => t('Is a Request?'),
      '#default_value' => ($batch && isset($batch['is_batch_request']))? $batch['is_batch_request'] : '',
    ),
    // Requestor: textfield
    'batch_requestor' => array(
      '#type' => 'textfield',
      '#title' => t('Requestor'),
      '#default_value' => ($batch && isset($batch['batch_requestor']))? $batch['batch_requestor'] : '',
    ),
    // Request due date: textfield
    'batch_request_due_date' => array(
//      '#type' => 'textfield',
      '#type' => 'date',
      '#default_value' => array(
        'month' => format_date((($batch && isset($batch['batch_request_due_date']))? strtotime($batch['batch_request_due_date']) : time()), 'custom', 'n'), 
        'day' => format_date((($batch && isset($batch['batch_request_due_date']))? strtotime($batch['batch_request_due_date']) : time()), 'custom', 'j'), 
        'year' => format_date((($batch && isset($batch['batch_request_due_date']))? strtotime($batch['batch_request_due_date']) : time()), 'custom', 'Y'),
       ),
      '#title' => t('Request Due Date'),
    ),
    // Condition handling: textarea
    'batch_condition_handling' => array(
      '#type' => 'textarea',
      '#title' => t('Condition Handling'),
      '#default_value' => ($batch && isset($batch['batch_condition_handling']))? $batch['batch_condition_handling'] : '',
    ),
    // File type: textfield
    'file_type' => array(
      '#type' => 'textfield',
      '#title' => t('File Type'),
      '#default_value' => ($batch && isset($batch['file_type']))? $batch['file_type'] : '',
    ),
    // File naming scheme: textfield
    'file_naming_scheme' => array(
      '#type' => 'textfield',
      '#title' => t('File Naming Scheme'),
      '#default_value' => ($batch && isset($batch['file_naming_scheme']))? $batch['file_naming_scheme'] : '',
    ),
    // Image resolution in ppi: textfield
    'image_resolution' => array(
      '#type' => 'textfield',
      '#title' => t('Image Resolution in ppi'),
      '#default_value' => ($batch && isset($batch['image_resolution']))? $batch['image_resolution'] : '',
    ),
    // Image color type and bit depth: textfield
    'image_color_type_and_bitdepth' => array(
      '#type' => 'textfield',
      '#title' => t('Image Color Type and Bit Depth'),
      '#default_value' => ($batch && isset($batch['image_color_type_and_bitdepth']))? $batch['image_color_type_and_bitdepth'] : '',
    ),
    // Output target size: textfield
    'output_target_size' => array(
      '#type' => 'textfield',
      '#title' => t('Output target size'),
      '#default_value' => ($batch && isset($batch['output_target_size']))? $batch['output_target_size'] : '',
    ),
    // Page edge treatment: textarea
    'page_edge_treatment' => array(
      '#type' => 'textarea',
      '#title' => t('Page Edge Treatment'),
      '#default_value' => ($batch && isset($batch['page_edge_treatment']))? $batch['page_edge_treatment'] : '',
    ),
    // Use a color target? checkbox
    'use_color_target' => array(
      '#type' => 'checkbox',
      '#title' => t('Use Color Target?'),
      '#default_value' => ($batch && isset($batch['use_color_target']))? $batch['use_color_target'] : '',
    ),
    // Blank and missing page treatment: textarea
    'blank_and_missing_treatment' => array(
      '#type' => 'textarea',
      '#title' => t('Blank and Missing Page Treatment'),
      '#default_value' => ($batch && isset($batch['blank_and_missing_treatment']))? $batch['blank_and_missing_treatment'] : '',
    ),

    // Image editing treatment: textarea
    'image_editing_treatment' => array(
      '#type' => 'textarea',
      '#title' => t('Image Editing Treatment'),
      '#default_value' => ($batch && isset($batch['image_editing_treatment']))? $batch['image_editing_treatment'] : '',
    ),

    // Structural metadata treatment: textarea
    'structural_metadata_treatment' => array(
      '#type' => 'textarea',
      '#title' => t('Description'),
      '#default_value' => ($batch && isset($batch['structural_metadata_treatment']))? $batch['structural_metadata_treatment'] : '',
    ),

    // Default voyager id: textfield
    'default_voyager_id' => array(
      '#type' => 'textfield',
      '#title' => t('Default Voyager ID'),
      '#default_value' => ($batch && isset($batch['default_voyager_id']))? $batch['default_voyager_id'] : '',
    ),
    // Default EAD id: textfield
    'default_ead_id' => array(
      '#type' => 'textfield',
      '#title' => t('Default EAD_id'),
      '#default_value' => ($batch && isset($batch['default_ead_id']))? $batch['default_ead_id'] : '',
    ),

    // Default copyright status: select (hard-coded options)
    'batch_default_CR_status' => array(
      '#type' => 'select',
      '#title' => t('Default Copyright Status'),
      '#options' => array(''=>'Select Default Copyright Status',
        'copyrighted' => 'copyrighted',
        'pd' => 'public domain',
        'pd_usfed' => 'public domain - us federal document',
        'pd_holder' => 'public domain - dedicated by rights holder',
        'pd_expired' => 'public domain - expired copyright',
        'unknown' => 'unknown',
      ),
      '#default_value' => ($batch && isset($batch['batch_default_CR_status']))? $batch['batch_default_CR_status'] : '',
    ),

    // Default publication status: select (hard-coded options)
    'batch_default_pub_status' => array(
      '#type' => 'select',
      '#title' => t('Default Publication Status'),
      '#options' => array(''=>'Select Default Publication Status',
        'published' => 'published',
        'unpublished' => 'unpublished',
        'unknown' => 'unknown',
      ),
      '#default_value' => ($batch && isset($batch['batch_default_pub_status']))? $batch['batch_default_pub_status'] : '',
    ),

    // Default copyright holder name: textarea
    'batch_default_CR_holder' => array(
      '#type' => 'textarea',
      '#title' => t('Default Copyright Holder Name'),
      '#default_value' => ($batch && isset($batch['batch_default_CR_holder']))? $batch['batch_default_CR_holder'] : '',
    ),
    // Default permission notes: textarea
    'batch_default_perm_notes' => array(
      '#type' => 'textarea',
      '#title' => t('Default Permission Notes'),
      '#default_value' => ($batch && isset($batch['batch_default_perm_notes']))? $batch['batch_default_perm_notes'] : '',
    ),

    // Default depositor: textfield
    'default_depositor' => array(
      '#type' => 'textfield',
      '#title' => t('Default Depositor'),
      '#default_value' => ($batch && isset($batch['default_depositor']))? $batch['default_depositor'] : '',
    ),
    // Default type of resource: textfield -- THIS SHOULD BE A SELECT BOX
    'default_type_of_resource' => array(
      '#type' => 'textfield',
      '#title' => t('Default Type of Resource'),
      '#default_value' => ($batch && isset($batch['default_type_of_resource']))? $batch['default_type_of_resource'] : '',
    ),
    // Default genre: textfield
    'default_genre' => array(
      '#type' => 'textfield',
      '#title' => t('Default Genre'),
      '#default_value' => ($batch && isset($batch['default_genre']))? $batch['default_genre'] : '',
    ),

    // Has file: checkbox
    'has_file' => array(
      '#type' => 'checkbox',
      '#title' => t('Has File?'),
      '#default_value' => ($batch && isset($batch['has_file']))? $batch['has_file'] : '',
    ),

    // File: file
    'file' => array(
      '#type' => 'file',
      '#title' => t('File'),
      '#description' => ($batch && isset($batch['file'])) ? '<i>saved value: ' . str_replace('/ingest/files/', '', $batch['file']) . '</i>' : '',
    ),

    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save batch'),
    ),
  );

  return $form;
}

function upitt_workflow_new_batch_form_submit(array $form, array &$form_state) {
  global $user;

  drupal_set_message('All done with form_submit code for workflow/batch/new form.');

  $batch_id = upitt_workflow_save_batch($form_state['values']);
  $batch_external_id = $form_state['values']['batch_external_id'];

  // ONLY create a new drupal node if this record does not yet have a nid value.
  if (isset($form_state['values']['nid']) && $form_state['values']['nid'] > 0) {
    $node = node_load($form_state['values']['nid']);
  }
  else {
    $node = new stdClass();
    $node->type = 'workflow_batch';
    $node->language = 'en';
    node_object_prepare($node);
    $node->title = 'Workflow batch ' . $batch_external_id;
    $body_markup = '<p>' . l('Manage', '/workflow/batch/' . $batch_id) . ' this batch.</p>' .
                   '<p><b>Description</b><br />' . $form_state['values']['batch_description'] . '</p>';
    $node->body['und'][0]['value'] = $body_markup;
    $node->body['und'][0]['summary'] = text_summary($body_markup);
    $node->body['und'][0]['format'] = 'full_html';

    $node->status = 1;   // (1 or 0): published or unpublished
    $node->promote = 0;  // (1 or 0): promoted to front page or not
    $node->sticky = 0;  // (1 or 0): sticky at top of lists or not
    $node->comment = 1;  // 2 = comments open, 1 = comments closed, 0 = comments hidden
    // Add author of the node
    $node->uid = $user->uid;
    // Set created date
    $current_date = date('H:i:s m/d/Y');
    $node->date = $current_date;
    $node->created = strtotime($current_date);

    $path = '/workflow_batch/' . $batch_external_id;
    $node->path = array('alias' => $path);

    $node = node_submit($node);
    node_save($node);
    drupal_set_message('Node created for workflow batch "' . $batch_external_id . '".  ' . l('Manage', '/workflow/batch/' . $batch_id) . ' this batch.');
    upitt_workflow_update_batch_nid($batch_id, $node);
  }

  dpm($node);
  $form_state['redirect'] = '/workflow/batch/' . $batch_id;
}


function upitt_workflow_save_batch($values) {
  $batch_id = upitt_workflow_get_batch_id($values['batch_external_id']);

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  // these form_state values are not needed for the SQL and would cause problems, so removed them here.
  unset($values['submit']);  unset($values['form_build_id']); unset($values['form_token']); unset($values['form_id']); unset($values['op']);
  // this includes the TINYINT fields used for the checkboxes
  $batch_integer_fields = array('use_color_target', 'is_batch_request', 'is_batch_active', 'has_file', 'batch_property_owner_id', 'batch_sequence_id', 'item_count', 'content_type_id');

  $batch_external_id = $values['batch_external_id'];

  $sql_fields = $sql_values = array();
  if ($batch_id) {
    $sql_fields[] = 'batch_id';
    $sql_values[] = $batch_id;
  }
  // assume all fields need to be escaped and wrapped with '' characters -- EXCEPT for the integer fields.
  foreach ($values as $fieldname => $value) {
    $sql_fields[] = $fieldname;

    // If the file is set, the actual file behind this must be moved to the /files/workflow folder and the field value must be set to this path.
    if ($fieldname == 'file' && isset($_FILES['files'])) {
      $dest = '/ingest/files/' . date('YmdHis') . '_' . $batch_external_id;
      if ($_FILES['files']['error']['file'] == UPLOAD_ERR_OK) {
        $tmp_name = $_FILES["files"]["tmp_name"]['file'];
        // basename() may prevent filesystem traversal attacks;
        // further validation/sanitation of the filename may be appropriate
        move_uploaded_file($tmp_name, $dest);
        $value = $dest;
      }
    }

    if (array_search($fieldname, $batch_integer_fields) === FALSE) {
      if ($fieldname == 'batch_request_due_date' || $fieldname == 'date') {
        $value = $value['month'] . '/' . $value['day'] . '/' . $value['year'];
      } elseif (is_array($value)) {
        $value = implode(",", $value);
      }
      $sql_values[] = "'" . mysql_real_escape_string($value) . "'";
    } else {
      $sql_values[] = $value;
    }
  }

  $sql = "REPLACE INTO `batch` (`" . implode("`, `", $sql_fields) . "`) VALUES (" . implode(", ", $sql_values) . ")";

  $result = mysql_query($sql, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysql_error() . "\n";
    $message .= 'Whole query: ' . $sql;
    die($message);
  }

  mysql_close($link);
  return upitt_workflow_get_batch_id($values['batch_external_id']);
}

function upitt_workflow_get_batch_id($batch_external_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $sql = 'SELECT `batch_id` FROM `batch` WHERE `batch_external_id` = "' . mysql_real_escape_string($batch_external_id) . '"';

  $result = mysql_query($sql, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysql_error() . "\n";
    $message .= 'Whole query: ' . $sql;
    die($message);
  }

  if ($row = mysql_fetch_assoc($result)) {
    $batch_id = $row['batch_id'];
  }
  else {
    $batch_id = 0;
  }

  mysql_close($link);
  return $batch_id;
}

function upitt_workflow_update_batch_nid($batch_id, $node) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $sql = 'UPDATE `batch` SET `nid` = ' . $node->nid . ' WHERE `batch_id` = ' . $batch_id;

  $result = mysql_query($sql, $link);

  mysql_close($link);
}

function upitt_workflow_batch_load($batch_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $batch_detail = 'SELECT * ' .
                  'FROM batch ' .
                  'WHERE batch_id = \'' . $batch_id . '\' LIMIT 1';

  $batch = array();

  $result = mysql_query($batch_detail, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysql_error() . "\n";
    $message .= 'Whole query: ' . $batch_detail;
    die($message);
  }

  $batch = $fields = array();
  if ($row = mysql_fetch_assoc($result)) {
    $fields = array_keys($row);
    foreach ($fields as $field) {
      $batch[$field] = $row[$field];
    }
  }

  mysql_close($link);
  return $batch;
}
