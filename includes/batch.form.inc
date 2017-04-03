<?php

function upitt_workflow_new_batch_form(array $form, array &$form_state, $batch = NULL) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('module', 'islandora_basic_collection', 'islandora_basic_collection');

  //  $batch_item_type_options = upitt_workflow_get_mysql_options('batch_item_type', 'Item Type', 'name', 'name', 'name', '0');
  //  $batch_property_owners = upitt_workflow_get_mysql_options('property_owner', 'Property Owner', 'property_owner_id', 'description', 'description', '0');
  //  $batch_workflow_sequences = upitt_workflow_get_mysql_options('workflow_sequence', 'Workflow Sequence', 'id', 'name', 'id', '0');
  $sites = upitt_workflow_get_mysql_options('wflocal_fedora_site', 'Site', 'pid', 'name', 'name', 0);

  // for legacy batches, this array may need to be joined with the legacy collection `c_id` pointers instead of Solr PID values.
  $collections = upitt_workflow_get_solr_options('RELS_EXT_hasModel_uri_ms:info\:fedora\/islandora\:collectionCModel', 'PID', 'fgs_label_s');
  asort($collections);
  $batch_id = ((is_numeric($batch)) ? $batch : upitt_workflow_get_batch_id($batch, FALSE));

  $fedora_object_mappings = upitt_workflow_get_workflow_to_fedora_object_mappings();
  $default_types_of_resource = array();
  // These values are actually the inverse of the keys/values from the mappings variable.  Flip-flop
  // them for the options parameter needed for the default_type_of_resource prompt.
  foreach ($fedora_object_mappings as $k => $v) {
    $default_types_of_resource[$k] = $k;
  }

  $read_only_att = array();
  $nid = '';
  // Load up a couple variables if there was a batch (id) passed into the form.
  if ($batch_id) {
    $batch = upitt_workflow_batch_load($batch_id);
    if (is_array($batch) && isset($batch['batch_external_id']) && $batch['batch_external_id']) {
      $read_only_att = array('readonly' => 'readonly', 'disabled' => 'disabled');
      $nid = $batch['nid'];
    }
    $batch_path = upitt_workflow_batch_path($batch);
    $batch_external_id = (isset($batch['batch_external_id'])) ? $batch['batch_external_id'] : '';
  } else {
    $batch_path = rtrim(variable_get('upitt_workflow_ingest_path', '/ingest/incoming'), '/') . '/batch_0_not_named';
    $batch_external_id = '';
  }

  // Prepare a couple variables for use with file upload options:
  $upload_size = min((int) ini_get('post_max_size'), (int) ini_get('upload_max_filesize'));
  $extensions = array('csv');

  // Load the batch's file object into csv using file_header and file_rows for display below the form.
  $file_rows = $file_header = array();
  if (file_exists($batch['file'])) {
    $arr = upitt_workflow_csv_file_as_array($batch['file']);
    $file_header = (isset($arr['header']) ? $arr['header'] : array());
    $file_rows = (isset($arr['rows']) ? $arr['rows'] : array());
    $csv_header_mappings = upitt_workflow_get_csv_header_xpath_mappings();
    foreach ($file_header as $header) {
      $machine_header = str_replace(" ", "_", strtolower(trim($header)));
      if ($machine_header && !(array_key_exists($machine_header, $csv_header_mappings))) {
        drupal_set_message('CSV column "' . $header . '" not recognized.  Batch will not be able to ingest or process.  Please replace the attached CSV file "' . $batch['file'] . '".', 'error');
      }
    }
  }

  $batch_hosts = upitt_workflow_get_batch_host_values();

  $form = array(
    'breadcrumb' => array('#markup' => upitt_workflow_get_breadcrumb_path()),
    'nid' => array(
      '#type' => 'hidden',
      '#default_value' => $nid,
    ),
    // Name: textfield
    'batch_external_id' => array(
      '#type' => 'textfield',
      '#title' => t('Name'),
      '#attributes' => $read_only_att,
      '#description' => t('Legacy batch name, `batch_external_id`'),
      '#default_value' => $batch_external_id,
    ),

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
    'mapto_site_id_values' => array(
      '#type' => 'select',
      '#description' => 'Hold down Ctrl to select multiple sites',
      '#title' => t('Site'),
      '#multiple' => TRUE,
      '#options' => $sites,
      '#default_value' => ($batch && isset($batch['mapto_site_id_values']))? explode(",", $batch['mapto_site_id_values']) : '',
      '#size' => 10,
    ),
    // Default depositor: textfield
    'default_depositor' => array(
      '#type' => 'textfield',
      '#description' => 'This value will only be used if the spreadsheet MODS does not contain a value for the depositor.',
      '#title' => t('Default Depositor'),
      '#default_value' => ($batch && isset($batch['default_depositor']))? $batch['default_depositor'] : '',
    ),
    // Description: textarea
    'batch_description' => array(
      '#type' => 'textarea',
      '#title' => t('Description'),
      '#default_value' => ($batch && isset($batch['batch_description']))? $batch['batch_description'] : '',
    ),
    // Priority: textfield
    'batch_priority' => array(
      '#type' => 'textfield',
      '#title' => t('Priority'),
      '#default_value' => ($batch && isset($batch['batch_priority']))? $batch['batch_priority'] : '',
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
    // Default type of resource: select
    'default_type_of_resource' => array(
      '#type' => 'select',
      '#title' => t('Default Type of Resource'),
      '#options' => $default_types_of_resource,
      '#default_value' => ($batch && isset($batch['default_type_of_resource']))? $batch['default_type_of_resource'] : '',
    ),
    // Is a request? checkbox
    'batch_request' => array(
      '#title' => 'Batch Request?',
      '#type' => 'fieldset',
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
        '#type' => 'date',
        '#default_value' => array(
          'month' => format_date((($batch && isset($batch['batch_request_due_date']))? strtotime($batch['batch_request_due_date']) : time()), 'custom', 'n'),
          'day' => format_date((($batch && isset($batch['batch_request_due_date']))? strtotime($batch['batch_request_due_date']) : time()), 'custom', 'j'),
          'year' => format_date((($batch && isset($batch['batch_request_due_date']))? strtotime($batch['batch_request_due_date']) : time()), 'custom', 'Y'),
         ),
        '#title' => t('Request Due Date'),
      ),
    ),
    // Requestor: textfield
    'batch_host' => array(
      '#type' => 'select',
      '#title' => t('Ingest files from host'),
      '#options' => $batch_hosts,
      '#default_value' => ($batch && isset($batch['batch_host']))? $batch['batch_host'] : '',
      '#description' => t('Specify the location of the files that relate to the uploaded spreadsheet.')
    ),
    // File: file
    'file' => array(
      '#type' => 'file',
      '#title' => t('File'),
      '#description' => t('Select CSV file to upload.<br/>Files must be less than <strong>@size MB.</strong><br/>Allowed file types: <strong>@ext.</strong>', array('@size' => $upload_size, '@ext' => $extensions[0])) .
                        (($batch && isset($batch['file']) && ($batch['file'] <> '')) ? '<br>Upload file will be stored: <em>' . str_replace($batch_path, '', $batch['file']) . '</em>' : ''),
      '#upload_location' => 'temporary://',
      '#upload_validators' => array(
        'file_validate_extensions' => $extensions,
         // Assume its specified in MB.
        'file_validate_size' => array($upload_size * 1024 * 1024),
      ),
      '#suffix' => theme('table', array('rows' => $file_rows, 'header' => $file_header, 'attributes' => array('class' => array('report_table')))),
    ),

/*
    // Type: select (get options from `batch_item_type`)
    'batch_item_type_id' => array(
      '#type' => 'select',
      '#title' => t('Type'),
      '#options' => $batch_item_type_options,
      '#default_value' => ($batch && isset($batch['content_type_id']))? $batch['content_type_id'] : '',
    ),
*/
/*    // Property owner: select (get options from `property_owners`)
    'batch_property_owner_id' => array(
      '#type' => 'select',
      '#title' => t('Property Owner'),
      '#options' => $batch_property_owners,
      '#default_value' => ($batch && isset($batch['property_owner_id']))? $batch['property_owner_id'] : '',
    ),
 */
/*    // Sequence: select (get options from `workflow_sequence`)
    'batch_sequence_id' => array(
      '#type' => 'select',
      '#title' => t('Sequence'),
      '#options' => $batch_workflow_sequences,
      '#default_value' => ($batch && isset($batch['sequence_id']))? $batch['sequence_id'] : '',
    ), */
    // Active? checkbox
/*    'is_batch_active' => array(
      '#type' => 'checkbox',
      '#title' => t('Active?'),
      '#default_value' => ($batch && isset($batch['is_batch_active']))? $batch['is_batch_active'] : '',
    ),
 */
/*    // Source identifier: textfield
    'batch_source_identifier' => array(
      '#type' => 'textfield',
      '#title' => t('Source Identifier'),
      '#default_value' => ($batch && isset($batch['batch_source_identifier']))? $batch['batch_source_identifier'] : '',
    ), */
/*    // Condition handling: textarea
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
    ), */

/*
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
*/
/*
    // Default genre: textfield
    'default_genre' => array(
      '#type' => 'textfield',
      '#title' => t('Default Genre'),
      '#default_value' => ($batch && isset($batch['default_genre']))? $batch['default_genre'] : '',
    ),
*/
/*
    // Has file: checkbox
    'has_file' => array(
      '#type' => 'checkbox',
      '#title' => t('Has File?'),
      '#default_value' => ($batch && isset($batch['has_file']))? $batch['has_file'] : '',
    ),
*/
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save batch'),
    ),
  );

  return $form;
}

function upitt_workflow_new_batch_form_submit(array $form, array &$form_state) {
  global $user;
  $collection_options = $form_state['complete form']['mapto_collections']['#options'];

  $batch_id = upitt_workflow_save_batch($form_state['values'], $collection_options);
  $batch_external_id = $form_state['values']['batch_external_id'];
  drupal_set_message('Updated "' . $batch_external_id . '".');

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

/**
 * This will save or update the given batch record.  Special process will potentially need to populate the collection table if the choices are fedora collections.
 */
function upitt_workflow_save_batch($values, $collection_options) {
  global $user;
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $collections_values = $values['mapto_collections'];

  $batch_id = upitt_workflow_get_batch_id($values['batch_external_id'], TRUE);
  $existing_batch_record = ($batch_id) ? upitt_workflow_batch_load($batch_id) : array();

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  // these form_state values are not needed for the SQL and would cause problems, so removed them here.
  unset($values['submit']);  unset($values['form_build_id']); unset($values['form_token']); unset($values['form_id']); unset($values['op']);
  // this includes the TINYINT fields used for the checkboxes
  $batch_integer_fields = array('use_color_target', 'is_batch_request', 'is_batch_active', 'has_file', 'batch_property_owner_id', 'batch_sequence_id', 'item_count', 'content_type_id');

  $batch_external_id = $values['batch_external_id'];

  if (!$batch_external_id) {
    drupal_set_message('The batch Name is a required field.  The batch could not be saved.', 'error');
    return;
  }
  $sql_fields = $sql_values = array();
  if ($batch_id) {
    $sql_fields[] = 'batch_id';
    $sql_values[] = $batch_id;
  }
  $batch_path = upitt_workflow_batch_path($values);
  // assume all fields need to be escaped and wrapped with '' characters -- EXCEPT for the integer fields.
  foreach ($values as $fieldname => $value) {
    // If the file is set, the actual file behind this must be moved to the incoming
    // ingest folder and the field value must be set to this path.
    if ($fieldname == 'file') {
      if (isset($_FILES['files']) && isset($_FILES["files"]["tmp_name"]['file'])) {
        $has_file = FALSE;
        $dest = $batch_path . '/metadata.csv';
        if ($_FILES['files']['error']['file'] == UPLOAD_ERR_OK) {
          $tmp_name = $_FILES["files"]["tmp_name"]['file'];
          if (move_uploaded_file($tmp_name, $dest)) {
            $value = $dest;
            $has_file = TRUE;
          }
          else {
            drupal_set_message(t('File move operation from "' . $tmp_name . '" to "' . $dest . '" failed'), 'error');
            return false;
            $value = NULL;
          }
        }
        if (($fieldname == 'file' && $value) || $fieldname <> 'file' || (!empty($existing_batch_record['file']))) {
          $sql_fields[] = $fieldname;
          if (!$value && isset($existing_batch_record['file']) && $fieldname == 'file') {
            $value = $existing_batch_record['file'];
            $has_file = TRUE;
          }
        }
      }
    } else {
      $sql_fields[] = $fieldname;
    }

    if ($fieldname == 'mapto_collections') {
      upitt_workflow_sync_solr_collections($value, $collection_options);
    }

    if (!is_null($value)) {
      if (array_search($fieldname, $batch_integer_fields) === FALSE) {
        if ($fieldname == 'batch_request_due_date' || $fieldname == 'date') {
          $value = $value['month'] . '/' . $value['day'] . '/' . $value['year'];
        } elseif (is_array($value)) {
          $value = implode(",", $value);
        }
        // Don't add any update to file value if it is not set from previous batch editing.
        if (($fieldname == 'file' && $value) || $fieldname <> 'file') {
          $sql_values[] = "'" . mysqli_real_escape_string($link, $value) . "'";
        }
      } else {
        $sql_values[] = $value;
      }
    }
  }
  $sql_fields[] = 'user';
  $sql_values[] = $user->uid;

  $sql = "REPLACE INTO `batch` (`" . implode("`, `", $sql_fields) . "`) VALUES (" . implode(", ", $sql_values) . ")";

  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  mysqli_close($link);
  return $batch_id;
}

function upitt_workflow_update_batch_nid($batch_id, $node) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $sql = 'UPDATE `batch` SET `nid` = ' . $node->nid . ' WHERE `batch_id` = ' . $batch_id;

  $result = mysqli_query($link, $sql);

  mysqli_close($link);
}
