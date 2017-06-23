<?php

/**
 * @file
 * Form to create batch records from a set of barcode values -- will lookup
 * the values for collection, site, and model from the legacy workflow system.
 */

// modes --
//  1) check objects in islandora
//  2) create or update batch records

function upitt_workflow_process_barcodes_form($form, &$form_state) {
  $process_options = array(
    'checkobjects' => 'Check for Islandora Objects',
    'update' => 'Update batch records (no overwrite if islandora object exists)',
    'updateforce' => 'Update batch records (force overwrite)',
    'do_copy' => 'Copy files from incoming to ingest location',
    'ingest_prepare' => 'Islandora Ingest Preprocess',
    'do_publish' => 'Publish objects to site');
  $form = array(
    'breadcrumb' => array('#markup' => upitt_workflow_get_breadcrumb_path()),
    'barcodes' => array(
      '#type' => 'textarea',
      '#title' => 'Barcodes',
      '#default_value' => isset($form_state['values']['barcodes']) ? $form_state['values']['barcodes'] : '',
      '#description' => 'Enter barcodes that correspond to object items from the legacy workflow system.  Each barcode should be on a separate line.'
    ),
    'mode' => array(
      '#title' => 'Processing mode',
      '#type' => 'select',
      '#options' => $process_options,
      '#default_value' => isset($form_state['values']['mode']) ? $form_state['values']['mode'] : '',
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Process barcodes'),
    ),
  );
  if(isset($form_state['storage'], $form_state['storage']['process_results'])) {
    $form['myowntext_display'] = array(
      '#markup' => $form_state['storage']['process_results'],
      '#prefix' => '',
      '#suffix' => '',
    );
  }
  return $form;
}

function upitt_workflow_process_barcodes_form_submit($form, &$form_state) {
  $markup = '';

  $btn_id = $form_state['triggering_element']['#id'];
  switch ($btn_id) {
    case 'edit-submit':
      if ($form_state['values']['mode'] == 'checkobjects') {
        $markup = upitt_workflow_process_check_islandora_for_barcodes($form_state['values']['barcodes']);
      }
      elseif ($form_state['values']['mode'] == 'ingest_prepare') {
        $markup = upitt_workflow_ingest_preprocess_from_barcodes($form_state['values']['barcodes']);
      }
      elseif ($form_state['values']['mode'] == 'do_copy') {
        $markup = upitt_workflow_do_copy_for_barcodes($form_state['values']['barcodes']);
      }
      elseif ($form_state['values']['mode'] == 'ingest_prepare') {
        $markup = upitt_workflow_make_batch_records_from_barcodes($form_state['values']['barcodes'], $form_state['values']['mode']);
      } elseif ($form_state['values']['mode'] == 'do_publish') {
        $markup = upitt_workflow_do_publish_for_barcodes($form_state['values']['barcodes']);
      }
      break;
    
    default:
      drupal_set_message('Triggering element "' . $btn_id . '" is not supported.');
      break;
  }
  if ($markup) {
    $form_state['rebuild'] = TRUE;
    $form_state['storage']['process_results'] = theme('upitt_simple_markup', array('markup' => $markup));
  }
}

/**
 * This will check whether or not each barcode is an islandora object.
 *
 * @param string $barcodes - will be split into an array
 * @return markup HTML
 */
function upitt_workflow_process_check_islandora_for_barcodes($barcodes) {
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $markup = '<h3>Found in Islandora</h3><pre class="bad">' . implode("\n", $check_results['found_pids']) . '</pre><hr>' .
            '<h3>NOT found in Islandora</h3><pre class="good">' . implode("\n", $check_results['not_found_pids']) . '</pre>';
  return $markup;
}

/**
 * This returns an associative array of:
 *   barcodes_arr = barcode values,
 *   found_pids = islandora pid values found in islandora,
 *   not_found_pids = islandora pid values NOT found in islandora
 * 
 * @param string $barcodes
 * @return associative array
 */
function upitt_workflow_check_islandora_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $found_pids = $not_found_pids = array();
  $barcodes_arr = explode("\n", $barcodes);
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    if ($barcode) {
      $pid = $ingest_namespace . $barcode;
      $islandora_object = islandora_object_load($pid);
      if (!is_object($islandora_object)) {
        $not_found_pids[$barcode] = $pid;
      }
      else {
        $found_pids[$barcode] = $pid;
      }
    }
  }
  return array('barcodes_arr' => $barcodes_arr, 'found_pids' => $found_pids, 'not_found_pids' => $not_found_pids);
}

/**
 * This function will actually create the batch records for the barcodes provided --
 * if the mode is set to updateforce, it will overwrite the existing islandora object on
 * ingest.
 *
 * (FROM AN EMAIL BY abrenner) following collections wouldn’t go into Fedora:
 * 
 *   ascead (finding aids are going in, but this should not be the record of them, nor is the content coming from bigfoot)
 *   ascead-test (test version of finding aids as above)
 *   cleftpalate (moving to OJS)
 *   hswpead (history center finding aids, same as ascead above)
 *   hswpead-test (test version of above)
 *   internal_requests
 *   mali (was a very small collection Rush Miller arranged; I don’t believe we are carrying it forward)
 *   ncjw (in the workflow but media content is not on bigfoot)
 *   outside_requests
 *   pittpress_quarantine (these could potentially go into Fedora but be kept dark. They are books we digitized but authors did not allow them to go online)
 *
 *
 * @param string $barcodes - will be split into an array
 * @param string $mode
 * @return markup HTML
 */
function upitt_workflow_make_batch_records_from_barcodes($barcodes, $mode) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  global $user;

  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $markup = '';
  if ($mode == 'updateforce') {
    $markup .= '<h2>Update batch records (force overwrite)</h2>';
    $use_pids = $check_results['found_pids'] + $check_results['not_found_pids']; // array_merge($check_results['found_pids'], $check_results['not_found_pids']);
    asort($use_pids);
  }
  else {
    $markup .= '<h2>Update batch records (no overwrite if islandora object exists)</h2>';
    $use_pids = $check_results['not_found_pids'];
  }
  $markup .= '<h2>Process these Islandora PID values</h2><pre style="color:blue">' . implode("\n", $use_pids) . '</pre><hr>';

  $markup .= '<h2>Found in Islandora</h2><pre style="color:red">' . implode("\n", $check_results['found_pids']) . '</pre>' .
            '<h2>NOT found in Islandora</h2><pre style="color:green">' . implode("\n", $check_results['not_found_pids']) . '</pre>';

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $toggle = FALSE;
  $sqls = array();
  $overwrite_if_exists = (($mode == 'updateforce') ? 1 : 0);
  foreach ($use_pids as $barcode => $pid) {
    $toggle = !$toggle;
    $markup .= "<div style='border-bottom: 1px solid black; padding-bottom: 8px; background-color:#" . ($toggle ? 'FFF' : 'F3F3F3') . "'>";
    $markup .= "  <br>barcode '" . $barcode . "' = islandora PID '" . $pid . "'<br>";
    $type = upitt_workflow_wf_item_get_type($barcode, $link);
    $model = upitt_workflow_wf_type_to_fedora_model($type);
    $batch_external_id_and_description = upitt_workflow_wf_item_get_name_and_desc($barcode, $link);
    $description = $barcode . ' from legacy batch named `' . $batch_external_id_and_description['name'] . '` ' . $batch_external_id_and_description['description'];
    $collections = upitt_workflow_wf_item_get_collection($barcode, $link);
    $sites = upitt_workflow_wf_item_get_site($barcode, $link);
    $markup .= 'type = <b>' . $type . "</b><br>";
    $markup .= 'description = <b>' . $description . "</b><br>";
    $markup .= 'collections = <b>' . $collections . "</b><br>";
    $markup .= 'sites = <b>' . $sites . "</b><br>";
    $markup .= "</div>";



    $sql = "REPLACE INTO `batch` (batch_external_id, " .
           "mapto_collections, " .
           "batch_description, " .
           "default_type_of_resource, " .
           "mapto_site_id_values, " .
           "overwrite_if_exists, " .
           "batch_host, " .
           "user) VALUES (" .
           "'" . mysqli_real_escape_string($link, $barcode) . "', " .
           "'" . mysqli_real_escape_string($link, $collections) . "', " .
           "'" . mysqli_real_escape_string($link, $description) . "', " .
           "'" . mysqli_real_escape_string($link, $model) . "', " .
           "'" . mysqli_real_escape_string($link, $sites) . "', " .
           $overwrite_if_exists . ", " .
           "'" . variable_get('upitt_workflow_ingest_incoming', '/ingest/incoming') . "'," .
           $user->uid . ");";
    mysqli_query($link, $sql);
    $sqls[$barcode] = $sql;
  }

  mysqli_close($link);
  return $markup;
}

function upitt_workflow_do_publish_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'upitt_workflow', 'includes/batch.form');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    // 1. load the batch record for the given barcode - and pass to the function that does the copy operation
    $batch = upitt_workflow_batch_load_by_batch_external_id($barcode);
    if (count($batch) < 1) {
      $missing_barcodes[] = $barcode;
    }
    $markup .= '<div>' . upitt_workflow_do_publish_using_batchvalues($batch, $ingest_namespace) . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . print_r($missing_barcodes, true), 'warning');
  }
  return $markup;
}

function upitt_workflow_do_publish_using_batchvalues($batch, $ingest_namespace) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $object_id = $batch['batch_external_id'];
  $sites = $batch['mapto_site_id_values'];
  $collections = $batch['mapto_collections'];

  $sites_arr = explode(",", $sites);
  $collections_arr = explode(",", $collections);

  if (count($sites_arr) > 0 || count($collections_arr) > 0) {
    // 1. load the islandora_object - and loop through collections and sites to add
    // these relationships using the upitt_workflow_addRelationship - which will ensure
    // that duplicate relationships are not added.
    $pid = $ingest_namespace . $object_id;
    $object = islandora_object_load($pid);

    foreach ($sites_arr as $site) {
      $object->relationships->remove('http://digital.library.pitt.edu/ontology/relations#', 'isMemberOfSite', $site);
      $object->relationships->add('http://digital.library.pitt.edu/ontology/relations#', 'isMemberOfSite', $site);
    }

    foreach ($collections_arr as $collection) {
      $object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection);
      $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection);
    }

    upitt_workflow_set_status_for_batch($object_id, UPITT_WORKFLOW_ACTION_OBJECT_PUBLISHED_ON_SITE, NULL);
    return 'Published "' . $object_id . '" to sites and collections.';
  }
  else {
    drupal_set_message('There were no sites or collections related to the object "' . $object_id . '"', 'warning');
  }
}

function upitt_workflow_do_copy_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'upitt_workflow', 'includes/batch.form');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '';
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    // 1. load the batch record for the given barcode - and pass to the function that does the copy operation
    $batch = upitt_workflow_batch_load_by_batch_external_id($barcode);
    if (count($batch) < 1) {
      $missing_barcodes[] = $barcode;
    }
    $do_copy = TRUE;
    $do_download = (strstr($batch['batch_host'], 'ftp:'));
    $content_model = isset($batch['default_type_of_resource']) ? $batch['default_type_of_resource'] : 'sp_large_image_cmodel';
    $batch_external_id = $batch['batch_external_id'];
    $incoming_folder = $batch_external_id;

    if (!$incoming_folder || !$batch_external_id) {
      drupal_set_message('The batch name and a incoming folder must be set in order to save this batch', 'error');
      return;
    }

    // Now it is safe to make the full path to the specific incoming subfolder that has been selected.
    $incoming_folder = (isset($batch['batch_host']) && strstr($batch['batch_host'], 'ftp:')) ?
            $batch['batch_host'] :
            rtrim(variable_get('upitt_workflow_ingest_incoming', '/ingest/incoming'), '/') . '/' . $incoming_folder;
    $batch_path = rtrim(variable_get('upitt_workflow_ingest_prepared_path', '/ingest/islandora_ingest'), '/') . '/batch_' . $batch_external_id;

    $markup .= '<div>' . upitt_workflow_copyfiles_incoming_folder_to_batch($incoming_folder, $batch_path, $content_model, $do_copy, $do_download, $batch_external_id) . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . print_r($missing_barcodes, true), 'warning');
  }
  return $markup;
}

function upitt_workflow_ingest_preprocess_from_barcodes($barcodes) {
  global $user;
  $barcodes_arr = explode("\n", $barcodes);

  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  $markup = '';
  $markup .= '<h2>Islandora Ingest Preprocess for These Barcodes</h2><pre style="color:blue">' . implode("\n", $barcodes_arr) . '</pre><hr>';
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    // Need to get the related batch record to determine whether or not the object
    // is ingested, the related model to use for ingest prepare call...
    $batch_id = upitt_workflow_get_batch_id($barcode, FALSE);
    if ($batch_id) {
      $batch = upitt_workflow_batch_load($batch_id);
      if (isset($batch['batch_id'])) {
        // TODO -- can load the status transactions of this batch to see whether
        // or not the object has been preprocessed or ingested already.
        $markup .= upitt_workflow_drush_generic($batch);
      }
      else {
        $markup .= '<span class="bad">batch not found for $barcode</span><br>';
      }
    }
    else {
      $markup .= '<span class="bad">batch_id could not be found for ' . $barcode . '</span><br>';
    }
  }
  return $markup;
}


/**
 * This will run a query to get the type of the item (barcode) from the legacy
 * workflow django tables.
 *
 * -- get type of object - maps to fedora models
 * SELECT it.name
 * FROM item i
 * JOIN item_type it ON (it.id = i.type_id) where i.do_id = '31735062218320';
 *
 * @param string $barcode
 * @param type $link
 */
function upitt_workflow_wf_item_get_type($barcode, $link) {
  $sql = "SELECT it.name `name` FROM item i JOIN item_type it ON (it.id = i.type_id) where i.do_id = '" . mysqli_real_escape_string($link, $barcode) . "'";
  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $row = mysqli_fetch_assoc($result);
  if (isset($row['name'])) {
    return $row['name'];
  }
}

/**
 *
 * @param type $barcode
 * @param type $link
 */
function upitt_workflow_wf_item_get_name_and_desc($barcode, $link) {
  $sql = "SELECT b.batch_external_id `name`, b.batch_description `description` " . 
         "FROM item i " . 
         "JOIN batch_item bi ON (bi.item_id = i.id) " .
         "JOIN batch b ON (b.batch_id = bi.batch_id) " . 
         "WHERE i.do_id = '" . mysqli_real_escape_string($link, $barcode) . "' LIMIT 1";

  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $row = mysqli_fetch_assoc($result);
  return array('name' => (isset($row['name'])) ? $row['name'] : '',
      'description' => (isset($row['description'])) ? $row['description'] : '');
}

/**
 * This will run a query to get the collection of the item (barcode) from the legacy
 * workflow django tables.
 *
 * -- get collection from workflow system
 * SELECT wfc.pid
 * FROM item i
 * JOIN wflocal_local_item_fedora_collections wfifc ON (wfifc.local_item_id = i.id)
 * JOIN wflocal_fedora_collection wfc ON (wfc.id = wfifc.fedora_collection_id)
 * WHERE i.do_id = '31735062218320';
 *
 * @param type $barcode
 * @param type $link
 */
function upitt_workflow_wf_item_get_collection($barcode, $link) {
  $sql = "SELECT wfc.pid `pid` " .
         "FROM item i " .
         "JOIN wflocal_local_item_fedora_collections wfifc ON (wfifc.local_item_id = i.id) " .
         "JOIN wflocal_fedora_collection wfc ON (wfc.id = wfifc.fedora_collection_id) " .
         "WHERE i.do_id = '" . mysqli_real_escape_string($link, $barcode) . "'";

  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['pid'])) {
      $rows[] = $row['pid'];
    }
  }
  return implode(",", $rows);
}

/**
 * This will run a query to get the site of the item (barcode) from the legacy
 *w orkflow django tables.
 *
 * -- get site from workflow system
 * SELECT wfs.pid
 * FROM item i
 * JOIN wflocal_local_item_fedora_sites wfifc ON (wfifc.local_item_id = i.id)
 * JOIN wflocal_fedora_site wfs ON (wfs.id = wfifc.fedora_site_id)
 * WHERE i.do_id = '31735062218320';
 *
 * @param type $barcode
 * @param type $link
 */
function upitt_workflow_wf_item_get_site($barcode, $link) {
  $sql = "SELECT wfs.pid `pid` " .
         "FROM item i " .
         "JOIN wflocal_local_item_fedora_sites wfifc ON (wfifc.local_item_id = i.id) " .
         "JOIN wflocal_fedora_site wfs ON (wfs.id = wfifc.fedora_site_id) " .
         "WHERE i.do_id = '" . mysqli_real_escape_string($link, $barcode) . "'";

  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['pid'])) {
      $rows[] = $row['pid'];
    }
  }
  return implode(",", $rows);
}
