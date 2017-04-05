<?php

function upitt_workflow_folder_to_batch_form(array $form, array &$form_state) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('module', 'islandora_basic_collection', 'islandora_basic_collection');

  $sites = upitt_workflow_get_mysql_options('wflocal_fedora_site', 'Site', 'pid', 'name', 'name', 0);

  // for legacy batches, this array may need to be joined with the legacy collection `c_id` pointers instead of Solr PID values.
  $collections = upitt_workflow_get_solr_options('RELS_EXT_hasModel_uri_ms:info\:fedora\/islandora\:collectionCModel', 'PID', 'fgs_label_s');
  asort($collections);

  $fedora_object_mappings = upitt_workflow_get_workflow_to_fedora_object_mappings();
  $default_types_of_resource = array();
  // These values are actually the inverse of the keys/values from the mappings variable.  Flip-flop
  // them for the options parameter needed for the default_type_of_resource prompt.
  foreach ($fedora_object_mappings as $k => $v) {
    $default_types_of_resource[$k] = $k;
  }

  $nid = '';
  $batch_path = rtrim(variable_get('upitt_workflow_ingest_path', '/ingest/incoming'), '/') . '/batch_0_not_named';
  $batch_external_id = '';
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

  $batch_hosts = upitt_workflow_get_batch_host_values();

  $form = array(
    'breadcrumb' => array('#markup' => upitt_workflow_get_breadcrumb_path()),
    'nid' => array(
      '#type' => 'hidden',
      '#default_value' => $nid,
    ),
    // Default type of resource: select
    'default_type_of_resource' => array(
      '#type' => 'select',
      '#title' => t('Default Type of Resource'),
      '#options' => $default_types_of_resource,
      '#default_value' => '',
    ),
    'source_holdings_folder' => array(
      '#type' => 'select',
      '#description' => 'Select the holdings folder that this batch should convert to an Islandora batch.',
      '#title' => t('Holdings folder'),
      '#options' => $holdings_folders,
      '#default_value' => '',
      '#size' => 10,
    ),
    'do_copy' => array(
      '#type' => 'checkbox',
      '#title' => t('Copy the files?'),
      '#description' => t('YES = copy the files, NO = inspect the files, but do not copy'),
      '#default_value' => '0',
    ),
    // Name: textfield
    'batch_external_id' => array(
      '#type' => 'textfield',
      '#title' => t('Name'),
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
      '#default_value' => '',
      '#size' => 10,
    ),
    'mapto_site_id_values' => array(
      '#type' => 'select',
      '#description' => 'Hold down Ctrl to select multiple sites',
      '#title' => t('Site'),
      '#multiple' => TRUE,
      '#options' => $sites,
      '#default_value' => '',
      '#size' => 10,
    ),
    // Default depositor: textfield
    'default_depositor' => array(
      '#type' => 'textfield',
      '#description' => 'This value will only be used if the spreadsheet MODS does not contain a value for the depositor.',
      '#title' => t('Default Depositor'),
      '#default_value' => '',
    ),
    // Description: textarea
    'batch_description' => array(
      '#type' => 'textarea',
      '#title' => t('Description'),
      '#default_value' => '',
    ),
/*    // Priority: textfield
    'batch_priority' => array(
      '#type' => 'textfield',
      '#title' => t('Priority'),
      '#default_value' => '',
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
      '#default_value' => '',
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
      '#default_value' => '',
    ),
    // Is a request? checkbox
    'batch_request' => array(
      '#title' => 'Batch Request?',
      '#type' => 'fieldset',
      'is_batch_request' => array(
        '#type' => 'checkbox',
        '#title' => t('Is a Request?'),
        '#default_value' => '',
      ),
      // Requestor: textfield
      'batch_requestor' => array(
        '#type' => 'textfield',
        '#title' => t('Requestor'),
        '#default_value' => '',
      ),
      // Request due date: textfield
      'batch_request_due_date' => array(
        '#type' => 'date',
        '#default_value' => array(
          'month' => format_date(time(), 'custom', 'n'),
          'day' => format_date(time(), 'custom', 'j'),
          'year' => format_date(time(), 'custom', 'Y'),
         ),
        '#title' => t('Request Due Date'),
      ),
    ),
*/
    // Requestor: textfield
    'batch_host' => array(
      '#type' => 'select',
      '#title' => t('Ingest files from host'),
      '#options' => $batch_hosts,
      '#default_value' => '',
      '#description' => t('Specify the location of the files that relate to the uploaded spreadsheet.')
    ),
    // File: file
    'file' => array(
      '#type' => 'file',
      '#title' => t('File'),
      '#description' => t('Select CSV file to upload.<br/>Files must be less than <strong>@size MB.</strong><br/>Allowed file types: <strong>@ext.</strong>', array('@size' => $upload_size, '@ext' => $extensions[0])),
      '#upload_location' => 'temporary://',
      '#upload_validators' => array(
        'file_validate_extensions' => $extensions,
         // Assume its specified in MB.
        'file_validate_size' => array($upload_size * 1024 * 1024),
      ),
      '#suffix' => theme('table', array('rows' => $file_rows, 'header' => $file_header, 'attributes' => array('class' => array('report_table')))),
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save batch'),
    ),
  );

  return $form;
}

function upitt_workflow_folder_to_batch_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $links = '';
  $collection_options = $form_state['complete form']['mapto_collections']['#options'];
  $batch_external_id = $form_state['values']['batch_external_id'];
  $holdings_folder = $form_state['values']['source_holdings_folder'];
  $do_copy = isset($form_state['values']['do_copy']) ? $form_state['values']['do_copy'] : FALSE;

  $fedora_object_mappings = upitt_workflow_get_workflow_to_fedora_object_mappings();
  $cm_key = $form_state['values']['default_type_of_resource'];
  $content_model = isset($fedora_object_mappings[$cm_key]) ? $fedora_object_mappings[$cm_key] : 'sp_large_image_cmodel';

  if (!$holdings_folder || !$batch_external_id) {
    drupal_set_message('The batch name and a holdings folder must be set in order to save this batch', 'error');
    return;
  }
  // Now it is safe to make the full path to the specific holdings subfolder that has been selected.
  $holdings_folder = rtrim(variable_get('upitt_workflow_holdings_path', '/ingest/holding'), '/') . '/' . $holdings_folder;
  $batch_path = rtrim(variable_get('upitt_workflow_ingest_path', '/ingest/incoming'), '/') . '/batch_' . $batch_external_id;

  $markup = upitt_workflow_copyfiles_holding_folder_to_batch($holdings_folder, $batch_path, $content_model, $do_copy);

  unset($form_state['values']['source_holdings_folder']);
  unset($form_state['values']['do_copy']);
  $batch_id = upitt_workflow_save_batch($form_state['values'], $collection_options);

  drupal_set_message('Updated "' . $batch_external_id . '".');

  // ONLY create a new drupal node if this record does not yet have a nid value.
  if (isset($form_state['values']['nid']) && $form_state['values']['nid'] > 0) {
    $node = node_load($form_state['values']['nid']);
    upitt_workflow_node_body_for_batch($node, $batch_external_id, $batch_id, $form_state['values']['batch_description'], $markup);
    $node = node_submit($node);
    node_save($node);
    drupal_set_message('loaded node nid = ' . $form_state['values']['nid']);
  }
  else {
    $node = upitt_workflow_make_node_for_batch($batch_external_id, $batch_id, $form_state['values']['batch_description'], $markup);
    upitt_workflow_update_batch_nid($batch_id, $node);
    drupal_set_message('New node created ' . $node->nid);
  }
//  $form_state['redirect'] = '/node/' . $node->nid;
  //  die($markup);
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
function upitt_workflow_copyfiles_holding_folder_to_batch($holdings_folder, $batch_path, $content_model, $do_copy = FALSE) {
  // with the folder of files, get the PID value from the {PID}.marcxml.xml file.
  $model_filemasks = upitt_workflow_content_model_filemask($content_model);

  // FOR sp_large_image_cmodel:
  //
  // 1) make a new folder under variable_get('upitt_workflow_ingest_path', '/ingest/incoming')
  //    named "batch_{PID}"
  //
  // 2) copy all relevant *.xml files and rename them to the assumed names (MODS.xml,
  //     --METADATA--.xml, METS.xml (?) (for marcxml))


  // FOR bookCModel:
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
    $keep_file = FALSE;
    // If the file has a subfolder reference, skip it
    if (strstr($file, '/')) {
      // SKIP
    }
    else {
      foreach ($model_filemasks as $mask => $model_filemask) {
        $keep_file |= upitt_workflow_filemask_comparison($file, $mask);
      }
      if ($keep_file) {
        $filemove_size += $this_size;
        $move_files[] = $file;
      }
    }
  }
  
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
    mkdir($batch_path . '/book');
    foreach ($move_files as $k => $file) {
      $file_mask_match = '';
      foreach ($model_filemasks as $mask => $model_filemask) {
        if (!$file_mask_match) {
          if (upitt_workflow_filemask_comparison($file, $mask)) {
            $file_mask_match = $mask;
          }
        }
      }
      /*    $filemasks['*.marcxml.xml'] = 'MARC XML';
            $filemasks['*.mets.xml'] = 'METS XML';
            $filemasks['*.tif'] = 'Page';
            $filemasks['*.pdf'] = 'PDF file';
       *
       */
      switch ($file_mask_match) {
        case '*.tif':
          if ($content_model == 'bookCModel') {
            // for Book, make directory, move file into that directory, rename OBJ.tif
            $file_baseName = str_replace(".tif", "", $file);
            mkdir($batch_path . '/book/' . $file_baseName);
            copy($holdings_folder . '/' . $file, $batch_path . '/book/' . $file_baseName . '/' . $file);
            rename($batch_path . '/book/' . $file_baseName . '/' . $file, $batch_path . '/book/' . $file_baseName . '/OBJ.tif');
            $markup .= "Moved and renamed '$file' to '" . $batch_path . "/book/" . $file_baseName . "/OBJ.tif'<br>";
          }
          else {
            // for large image, simply move file to root of batch_path
            copy($holdings_folder . '/' . $file, $batch_path . '/' . $file);
            $markup .= "Moved '$file' to '" . $batch_path . "/" . $file . "'<br>";
          }
          break;
        case '*.marcxml.xml':
          if ($content_model == 'bookCModel') {
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
          if ($content_model == 'bookCModel') {
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
    $markup .= "</pre>";
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
  $filemasks = array();
  switch ($content_model) {
    case 'sp_large_image_cmodel':
      // the MODS is created via CSV
      // $filemask['*.mods.xml'] = 'MODS XML';
      $filemasks['*.tif'] = 'Image';
      break;

    case 'bookCModel':
      $filemasks['*.marcxml.xml'] = 'MARC XML';
      $filemasks['*.mets.xml'] = 'METS XML';
      $filemasks['*.tif'] = 'Page';
      $filemasks['*.pdf'] = 'PDF file';
      break;

    default:
      break;

  }
  return $filemasks;
}

/**
 * This will return TRUE if the file should be kept based upon the filemask.
 *
 * @param string $file
 * @param string $filemask
 */
function upitt_workflow_filemask_comparison($file, $filemask) {
  $filemask_without_wildcards = str_replace("*", "", $filemask);
  $starts_with_wildcard = ($filemask[0] == '*');
  $ends_with_wildcard = ($filemask[strlen($filemask) - 1] == '*');

  if ($starts_with_wildcard && $ends_with_wildcard) {
    return (strstr($file, $filemask));
  }
  elseif ($starts_with_wildcard && !$ends_with_wildcard) {
    // the last few characters are the same as the mask, but not anything before the
    // wildcard, but MUST compare to the $filemask_without_wildcards
    return (substr($file, -strlen($filemask_without_wildcards)) == $filemask_without_wildcards);
  }
  elseif (!$starts_with_wildcard && $ends_with_wildcard) {
    // the first few characters are the same as the mask, but not anything after the
    // wildcard, but MUST compare to the $filemask_without_wildcards
    return (substr($file, 1, strlen($filemask_without_wildcards)) == $filemask_without_wildcards);
  }
  else {
    return ($file == $filemask);
  }
}