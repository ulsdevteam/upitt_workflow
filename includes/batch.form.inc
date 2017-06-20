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

    // Check to see if the batch folder has already created by an earlier batch or
    // by an earlier edit of this batch.  If so, warn the user.
    $existing_batch_files = upitt_workflow_file_scan_directory($batch_path, '', TRUE, FALSE);
  } else {
    $batch_path = rtrim(variable_get('upitt_workflow_ingest_path', '/ingest/incoming'), '/') . '/batch_0_not_named';
    $batch_external_id = '';
    $existing_batch_files = array();
  }

  $holdings_filepath = rtrim(variable_get('upitt_workflow_holdings_path', '/ingest/holding'), '/');
  $tmp_holdings_folders = upitt_workflow_file_scan_directory($holdings_filepath, '', FALSE, TRUE);
  // need the key to be the value as well
  foreach ($tmp_holdings_folders as $k => $holdings_folder) {
    $holdings_folders[$holdings_folder] = $holdings_folder;
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
  $count_existing_batch_files = upitt_workflow_array_flat_count_values($existing_batch_files);

  if ($batch['batch_host'] == 'ftp' || ltrim($batch['batch_host'], 'ftp:') <> $batch['batch_host']) {
    $show_or_hide['local_files'] = 'display:none';
    $show_or_hide['box_files'] = '';
    $current_batch_host_selection = 'ftp';
  }
  else {
    $show_or_hide['local_files'] = '';
    $show_or_hide['box_files'] = 'display:none';
    $current_batch_host_selection = ($batch && isset($batch['batch_host']))? $batch['batch_host'] : '';
  }
  $initial_ftp_path = ltrim(str_replace(
          array("ftp:", '/'),
          array("", '|'), $batch['batch_host']), '|');
  if ($initial_ftp_path == '') {
    $initial_ftp_path = 'Digital Collections Contributors';
  }
  $site = 'http://' . $_SERVER['HTTP_HOST'];

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
      '#suffix' => (($count_existing_batch_files > 0) ?
        '<em style="color:red">' .
        t('There are already ' . $count_existing_batch_files . ' files / folders in the batch folder.') . '</em>' .
        '<pre class="report_table_wrapper">' . print_r($existing_batch_files, true) . '</pre>' : ''),
    ),
    // Default type of resource: select
    'default_type_of_resource' => array(
      '#type' => 'select',
      '#title' => t('Model'),
      '#options' => $fedora_object_mappings,
      '#default_value' => ($batch && isset($batch['default_type_of_resource']))? $batch['default_type_of_resource'] : '',
    ),
    // Requestor: textfield
    'batch_host' => array(
      '#type' => 'select',
      '#title' => t('Ingest files from host'),
      '#options' => $batch_hosts,
      '#default_value' => $current_batch_host_selection,
      '#description' => t('Specify the location of the files that relate to the uploaded spreadsheet.'),
      '#attributes' => array(
        'onchange' => 'host_selected(this,"' . $site . '")'
      ),
    ),
    'initial_ftp_path' => array(
      '#type' => 'hidden',
      '#default_value' => $initial_ftp_path,
    ),
    'box_files_wrapper' => array(
      '#type' => 'fieldset',
      '#attributes' => array('style' => $show_or_hide['box_files']),
      'box_files' => array(
        '#type' => 'fieldset',
        '#title' => 'Box files',
        'loading' => array(
          '#markup' => '<div id="edit-box-files-loading-img"><h3>Loading results from FTP</h3><p>Please be patient...</p></div>',
        ),
      ),
      'do_download' => array(
        '#type' => 'checkbox',
        '#title' => t('Download and extract the ZIP files when batch is saved?'),
        '#default_value' => '1',
      ),
    ),
    'local_files' => array(
      '#type' => 'fieldset',
      '#title' => 'Local Files',
      '#attributes' => array('style' => $show_or_hide['local_files']),
      'source_holdings_folder' => array(
        '#type' => 'select',
        '#description' => 'Select the holdings folder that this batch should convert to an Islandora batch.',
        '#title' => t('Holdings folder'),
        '#options' => $holdings_folders,
        '#default_value' => $batch_external_id,
        '#size' => 10,
      ),
      'do_copy' => array(
        '#type' => 'checkbox',
        '#title' => t('Copy the files?'),
        '#description' => t('YES = copy the files, NO = inspect the files, but do not copy'),
        '#default_value' => '0',
      ),
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
/*
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
 */

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
      '#prefix' => "<a id='file_upload'> </a>",
      '#suffix' => (count($file_rows) ? '<div class="report_table_wrapper">' .
          theme('table', array('rows' => $file_rows, 'header' => $file_header, 'attributes' => array('class' => array('report_table')))) .
        '</div>'  : ''),
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

/**
 * This will perform the various checks to see whether or not the batch can be saved.
 *
 * @param array $elements
 * @param array $form_state
 * @param string $form_id
 */
function upitt_workflow_new_batch_form_validate(&$elements, &$form_state, $form_id = NULL) {
  if ($elements['default_type_of_resource']['#value'] == '') {
    form_set_error('default_type_of_resource', t('You have added an invalid Model.'));
  }
  // Now, if there is a file attached, the CSV validation can be performed
  
}

function upitt_workflow_new_batch_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $collection_options = $form_state['complete form']['mapto_collections']['#options'];

  if ($form_state['values']['batch_host'] == 'ftp') {
    $form_state['values']['batch_host'] = 'ftp:' . str_replace(
            array("|", "ftp:"),
            array("/", ""),
            $form_state['values']['initial_ftp_path']);
  }

  $holdings_folder = $form_state['values']['source_holdings_folder'];
  $do_copy = isset($form_state['values']['do_copy']) ? $form_state['values']['do_copy'] : FALSE;
  $do_download = isset($form_state['values']['do_download']) ? $form_state['values']['do_download'] : FALSE;
  $content_model = isset($form_state['values']['default_type_of_resource']) ? $form_state['values']['default_type_of_resource'] : 'sp_large_image_cmodel';
  $batch_external_id = $form_state['values']['batch_external_id'];

  if (!$holdings_folder || !$batch_external_id) {
    drupal_set_message('The batch name and a holdings folder must be set in order to save this batch', 'error');
    return;
  }

  // Now it is safe to make the full path to the specific holdings subfolder that has been selected.
  $holdings_folder = (isset($form_state['values']['batch_host']) && strstr($form_state['values']['batch_host'], 'ftp:')) ?
          $form_state['values']['batch_host'] :
          rtrim(variable_get('upitt_workflow_holdings_path', '/ingest/holding'), '/') . '/' . $holdings_folder;
  $batch_path = rtrim(variable_get('upitt_workflow_ingest_path', '/ingest/incoming'), '/') . '/batch_' . $batch_external_id;

  $markup = upitt_workflow_copyfiles_holding_folder_to_batch($holdings_folder, $batch_path, $content_model, $do_copy, $do_download, $batch_external_id);

  if ($do_copy) {
    upitt_workflow_set_status_for_batch($batch_external_id, UPITT_WORKFLOW_ACTION_HOLDING_FILES_MOVED, NULL);
  }

  // If these values are still set in the form_state, the save_batch code will not know what to do with them.
  unset($form_state['values']['source_holdings_folder']);
  unset($form_state['values']['do_copy']);
  unset($form_state['values']['do_download']);
  unset($form_state['values']['box_selector']);
  unset($form_state['values']['initial_ftp_path']);
  unset($form_state['values']['loading']);
  unset($form_state['values']['loading']);

  $batch_id = upitt_workflow_save_batch($form_state['values'], $collection_options);

  drupal_set_message('Updated "' . $batch_external_id . '" batch record.');

  // ONLY create a new drupal node if this record does not yet have a nid value.
  if (isset($form_state['values']['nid']) && $form_state['values']['nid'] > 0) {
    $node = node_load($form_state['values']['nid']);
    upitt_workflow_node_body_for_batch($node, $batch_external_id, $batch_id, $form_state['values']['batch_description']);
    $node = node_submit($node);
    node_save($node);
  }
  else {
    $node = upitt_workflow_make_node_for_batch($batch_external_id, $batch_id, $form_state['values']['batch_description']);
    upitt_workflow_update_batch_nid($batch_id, $node);
    drupal_set_message('New node created ' . $node->nid);
  }

  $form_state['redirect'] = '/workflow/batch/' . $batch_id;
}

/**
 * This function will take the contents of a folder that is in the holdings location
 * and make the files/folder structure that Islandora batch expects.
 *
 * @param string $holdings_folder - holdings folder, the "from" folder here
 * @param string $batch_path - the batch destination folder, the "to" folder here.
 * @param string $content_model represents which fedora model the batch folder represents
 * @param bool $do_copy - if FALSE, provides a report on the file size for the move,
 *          else, it copies the files as expected for an islandora batch.
 */
function upitt_workflow_copyfiles_holding_folder_to_batch($holdings_folder, $batch_path, $content_model, $do_copy = FALSE, $do_download = FALSE, $batch_external_id = '') {
  if (strstr($holdings_folder, 'ftp:') && $do_download) {
    // make a temp folder - unzip all files there - and set $holdings_folder to this
    // temp path -- and proceed as if they were local files.
    $temp_folder = tempnam(variable_get('file_temporary_path', '/tmp'), 'ftp_unzip');
    upitt_workflow_unzip_ftp_zipfiles_to_tempfolder($holdings_folder, $temp_folder, $batch_external_id);
    $holdings_folder = $temp_folder;
  }

  $model_filemasks = upitt_workflow_content_model_filemask($content_model);

  // with the folder of files, get the PID value from the {PID}.marcxml.xml file.

  // FOR sp_large_image_cmodel:
  //
  // 1) make a new folder under variable_get('upitt_workflow_ingest_path', '/ingest/incoming')
  //    named "batch_{PID}"
  //
  // 2) copy all relevant *.xml files and rename them to the assumed names (MODS.xml,
  //     --METADATA--.xml, METS.xml (?) (for marcxml))


  // FOR bookCModel or manuscriptCModel:
  //
  // 1) make a new folder under variable_get('upitt_workflow_ingest_path', '/ingest/incoming')
  //    named "batch_{PID}"
  //
  // 2) copy all relevant *.xml files and rename them to the assumed names (MODS.xml,
  //     --METADATA--.xml, METS.xml (?) (for marcxml))
  //
  // 3) make a subfolder for each page and copy the {page#}.tif to the folder as : {page#}/OBJ.tif

  $tmp_files = upitt_workflow_file_scan_directory($holdings_folder, '', TRUE, FALSE);
  $files = array();
  // FLATTEN this result
  foreach ($tmp_files as $key => $file) {
    if (is_array($file)) {
      foreach ($file as $subfile) {
        $files[] = $key . '/' . $subfile;
      }
    }
    else {
      $files[] = $file;
    }
  }
  $move_files = array();
  $size = $filemove_size = 0;
  foreach ($files as $i => $file) {
    $this_size = filesize($holdings_folder . '/' . $file);
    $size += $this_size;
    // If the file has a subfolder reference, skip it
    if (strstr($file, '/')) {
      // SKIP
    }
    else {
      $mask_matches = array();
      if ($matched_model_filemask = upitt_workflow_filemask_comparison($file, $model_filemasks)) {
        $move_files[$matched_model_filemask][] = $file;
      }
    }
  }

  $mods_file = '';
  if (!$do_copy) {
    $markup = "<h3>Report of copying files from '" . $holdings_folder . "' to '" . $batch_path . "'</h3>";

    $markup .= "<div><div style='float:left;width:48%'>";
    $markup .= "<h4>Size of all files in holding folder</h4>";
    $markup .= '<pre>' . number_format($size) . " bytes</pre>";
    $markup .= "  <pre>" . print_r($files, true) . "</pre>";
    $markup .= "</div>";

    $markup .= "<div style='float:left;width:48%'>";
    $markup .= "<h4>Size of all files to be moved</h4>";
    $markup .= '<pre>' . number_format($filemove_size) . " bytes</pre>";
    $markup .= "  <pre>" . print_r($move_files, true) . "</pre>";
    $markup .= "</div></div><br style='clear:both'>";
  }
  else {
    $markup = "<h3>Moving files from '" . $holdings_folder . "' to '" . $batch_path . "'</h3>";
    $markup .= "<pre>";

    if (file_exists($batch_path)) {
      $command = 'rm -rf ' . $batch_path;
      exec($command);
    }
    mkdir($batch_path);
    if ($content_model == 'bookCModel' || $content_model == 'manuscriptCModel') {
      mkdir($batch_path . '/book');
    }
    $obj_filename = '';

    foreach ($move_files as $file_mask_match => $files) {
      foreach ($files as $file) {
        switch ($file_mask_match) {
          case '*.tif'; case '*.jpg';
            if ($content_model == 'bookCModel' || $content_model == 'manuscriptCModel') {
              // for Book, make directory, move file into that directory, rename OBJ.tif
              $file_baseName = str_replace(str_replace("*", "", $file_mask_match), "", $file);
              $this_fileinfo = pathinfo($file);
              $extension = (isset($this_fileinfo['extension'])) ? $this_fileinfo['extension'] : '';
              if (strtolower($file_baseName) <> 'target') {
                $page_folder = $this_fileinfo['filename'];
                mkdir($batch_path . '/book/' . $page_folder);
                copy($holdings_folder . '/' . $file, $batch_path . '/book/' . $page_folder . '/' . $file);
                rename($batch_path . '/book/' . $page_folder . '/' . $file, $batch_path . '/book/' . $page_folder . '/OBJ.' . $extension);
                $markup .= "Moved and renamed '$file' to '" . $batch_path . "/book/" . $page_folder . "/OBJ." . $extension . "'<br>";
              }
            }
            else {
              // for large image, simply move file to root of batch_path
              $obj_filename = $file;
              copy($holdings_folder . '/' . $file, $batch_path . '/' . $file);
              $markup .= "Moved '$file' to '" . $batch_path . "/" . $file . "'<br>";
            }
            break;
          case '*.mods.xml':
            $mods_file = $file;
            break;
          case '*.marcxml.xml':
            if ($content_model == 'bookCModel' || $content_model == 'manuscriptCModel') {
              copy($holdings_folder . '/' . $file, $batch_path . '/book/' . $file);
              rename($batch_path . '/book/' . $file, $batch_path . '/book/--METADATA--.xml');
              $markup .= "Moved and renamed '$file' to '" . $batch_path . "/book/--METADATA--.xml'<br>";
            }
            else {
              copy($holdings_folder . '/' . $file, $batch_path . '/' . $file);
              rename($batch_path . '/' . $file, $batch_path . '/--METADATA--.xml');
              $markup .= "Moved and renamed '$file' to '" . $batch_path . "/--METADATA--.xml'<br>";
            }
            break;
          case '*.mets.xml':
            if ($content_model == 'bookCModel' || $content_model == 'manuscriptCModel') {
              copy($holdings_folder . '/' . $file, $batch_path . '/book/' . $file);
              rename($batch_path . '/book/' . $file, $batch_path .  '/book/METS.xml');
              $markup .= "Moved and renamed '$file' to '" . $batch_path . "/book/METS.xml'<br>";
            }
            else {
              copy($holdings_folder . '/' . $file, $batch_path . '/' . $file);
              rename($batch_path . '/' . $file, $batch_path .  '/METS.xml');
              $markup .= "Moved and renamed '$file' to '" . $batch_path . "/METS.xml'<br>";
            }
            break;
          default:
            break;
        }
      }
    }
    if ($mods_file) {
      if ($content_model == 'bookCModel' || $content_model == 'manuscriptCModel') {
        copy($holdings_folder . '/' . $mods_file, $batch_path . '/book/' . $mods_file);
        rename($batch_path . '/book/' . $mods_file, $batch_path . '/book/--METADATA--.xml');
        $markup .= "Moved and renamed '$mods_file' to '" . $batch_path . "/book/--METADATA--.xml'<br>";
      }
      else {
        copy($holdings_folder . '/' . $mods_file, $batch_path . '/' . $mods_file);
        // Each OBJ file has a corresponding XML file, and that the filenames of the OBJs and the XML files
        // are identical in every way – including capitalization – except for the extension based on:
        //   https://wiki.duraspace.org/display/ISLANDORA/How+to+Batch+Ingest+Files#HowtoBatchIngestFiles-IngestMetadataandFilestogether .
        if ($obj_filename) {
          $obj_pathinfo = pathinfo($obj_filename);
          if (isset($obj_pathinfo['filename'])) {
            $rename_to = $obj_pathinfo['filename'] . '.xml';
            rename($batch_path . '/' . $mods_file, $batch_path . '/' . $rename_to);
            $markup .= "Moved and renamed '$mods_file' to '" . $batch_path . "/" . $rename_to . "'<br>";
          }
        }
      }
    }
    $markup .= "</pre>";
  }

  // In either case, if the object is a book, and the --METADATA--.xml does not 
  // exist in the batch path, download the XML from voystaff.  This is only needed
  // if there is not a MODS file.
  if (($content_model == 'bookCModel' || $content_model == 'manuscriptCModel') && !$mods_file) {
    $marc_filename = $batch_path . '/book/--METADATA--.xml';
    if (!file_exists($marc_filename)) {
      // download the MARC file and save it... display / log the results.
      $markup .= '--METADATA--.xml did not exist<br>';
      $barcode = str_replace(rtrim(variable_get('upitt_workflow_holdings_path', '/ingest/holding'), '/') . '/', '', $holdings_folder);
      $voystaff_uri = 'http://voystaff.library.pitt.edu/cgi-bin/barcode2marcxml.php?barcode=' . $barcode;
      $xml = @file_get_contents($voystaff_uri);
      if ($xml && (strstr($xml, 'BibID not found for barcode') == '')) {
        $markup .= '<pre>' . htmlspecialchars(print_r($xml, true)) . '</pre>';
        $bytes_written = file_put_contents($marc_filename, $xml);
      } else {
        drupal_set_message('The MARC record could not be retrieved from the Voyager application. ' . l($voystaff_uri, $voystaff_uri) . '.' .
          ((strstr($xml, 'BibID not found for barcode') == '') ? '' : 'The BibID was not found for the barcode "' . $barcode . '".'), 'warning');
      }
    }    
  }

  return $markup;
}

/**
 * This will return an array of filemasks that should be kept / copied from the holding
 * folder based upon content_model.
 *
 * @param type $content_model
 */
function upitt_workflow_content_model_filemask($content_model) {
  $filemasks = $discard_filemasks = array();
  switch ($content_model) {
    case 'sp_large_image_cmodel':
      // the MODS is created via CSV
      $filemasks['*.mods.xml'] = 'MODS XML';
      $filemasks['*.tif'] = 'Image TIF';
      $filemasks['*.jpg'] = 'Image JPG';
      $discard_filemasks['*.thumb.jpg'] = 'old thumbnail file';
      $discard_filemasks['*.thumb_large.jpg'] = 'old large thumbnail file';
      $discard_filemasks['target.tif'] = 'graphics calibration';
      break;

    case 'bookCModel'; case 'manuscriptCModel';
      $filemasks['*.mods.xml'] = 'MODS XML';
      $filemasks['*.marcxml.xml'] = 'MARC XML';
      $filemasks['*.mets.xml'] = 'METS XML';
      $filemasks['*.tif'] = 'Page TIF';
      $filemasks['*.jpg'] = 'Page JPG';
      $filemasks['*.pdf'] = 'PDF file';
      $discard_filemasks['*.thumb.jpg'] = 'old thumbnail file';
      $discard_filemasks['*.thumb_large.jpg'] = 'old large thumbnail file';
      $discard_filemasks['target.tif'] = 'graphics calibration';
      break;

    default:
      break;

  }
  return array('keep_filemasks' => $filemasks, 'discard_filemasks' => $discard_filemasks);
}

/**
 * This will return the filematch that was matched if the file should be kept based
 * upon matching a single filemask.
 *
 * @param type $file
 * @param type $filemask
 * @return string
 */
function upitt_workflow_filemask_match($file, $filemask) {
  $d = ($file == '0001.thumb.jpg' && $filemask == '*.thumb.jpg');
  $filemask_without_wildcards = str_replace("*", "", $filemask);
  $starts_with_wildcard = ($filemask[0] == '*');
  $ends_with_wildcard = ($filemask[strlen($filemask) - 1] == '*');
  if ($starts_with_wildcard && $ends_with_wildcard) {
    $result = (strstr($file, $filemask));
  }
  elseif ($starts_with_wildcard && !$ends_with_wildcard) {
    // the last few characters are the same as the mask, but not anything before the
    // wildcard, but MUST compare to the $filemask_without_wildcards
    $result = (substr($file, -strlen($filemask_without_wildcards)) == $filemask_without_wildcards);
  }
  elseif (!$starts_with_wildcard && $ends_with_wildcard) {
    // the first few characters are the same as the mask, but not anything after the
    // wildcard, but MUST compare to the $filemask_without_wildcards
    $result = (substr($file, 1, strlen($filemask_without_wildcards)) == $filemask_without_wildcards);
  }
  else {
    $result = ($file == $filemask);
  }
  return ($result) ? $filemask : '';
}

/**
 * This will return either a blank string if the file should be discarded, or the filemask
 * that was used for keeping the file.
 *
 * @param string $file
 * @param string $filemask
 */
function upitt_workflow_filemask_comparison($file, $filemasks) {
  $discard = FALSE;
  foreach ($filemasks['discard_filemasks'] as $discard_mask => $discard_filemask_name) {
    if (!$discard) {
      $discard = upitt_workflow_filemask_match($file, $discard_mask);
    }
  }
  if ($discard) {
    return '';
  }
  $keep = FALSE;
  foreach ($filemasks['keep_filemasks'] as $keep_mask => $keep_filemask_name) {
    if (!$keep) {
      $keep = upitt_workflow_filemask_match($file, $keep_mask);
    }
  }
  return $keep;
}

function upitt_workflow_array_flat_count_values($array) {
  $count = 0;
  foreach ($array as $array_item) {
    if (is_array($array_item)) {
      $count += upitt_workflow_array_flat_count_values($array_item);
    }
    else {
      $count++;
    }
  }
  return $count;
}

function upitt_workflow_unzip_ftp_zipfiles_to_tempfolder($ftp_folder, $temp_folder, $batch_external_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/ftp_ops');
  if ($results_array = upitt_workflow_ftp_get_zipfiles_and_csv($ftp_folder, $temp_folder, $batch_external_id)) {
    if ($results_array['downloaded']) {
      drupal_set_message('There was ' . $results_array['num_zip_files_unzipped'] . ' zip file' . ($results_array['num_zip_files_unzipped'] == 1 ? '' : 's') .
        'downloaded from the FTP location `' . $ftp_folder . '`.' . "\n\n" . 
        'The batch record has been updated to point to the CSV file that was in the FTP folder `' . $results_array['csv_file'] . '`.' . "\n\n" .
        'The zip files were extracted to `' . $temp_folder . '` and contained ' . count($results_array['files_unzipped']) . ' file' .
        ($results_array['files_unzipped'] == 1 ? '' : 's') . '.  The raw files unzipped to ' . implode(",", $results_array['files_unzipped']));

      // TODO: move the csv file to the batch_holding directory along with all the
      // files ... and update the batch record using the batch_external_id
      
      $csv_file = $results_array['csv_file'];
    }
  }
}
