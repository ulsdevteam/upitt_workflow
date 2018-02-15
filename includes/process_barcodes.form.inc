<?php

set_time_limit(0);

/**
 * @file
 * Form to create batch records from a set of barcode values -- will lookup
 * the values for collection, site, and model from the legacy workflow system.
 */

// modes --
//  1) check objects in islandora
//  2) create or update batch records

function upitt_workflow_process_barcodes_form($form, &$form_state) {
  $admin = user_access('administer users');

  if (!isset($form_state['values']['barcodes']) && isset($_GET['barcodes']) && isset($_GET['mode'])) {
    $form_state['values']['barcodes'] = str_replace(array(';', ','), "\n", filter_xss($_GET['barcodes']));
    $form_state['values']['mode'] = filter_xss($_GET['mode']);
  }
  $process_options = array(
    '0' => 'Hopper Ingest Queue',
      'add_to_hopper' => 'Add to Hopper Ingest queue',
      'hopper_increase_priority' => 'Increase Priority',
      'hopper_decrease_priority' => 'Decrease Priority',
      'hopper_list_pending' => 'List Pending Hopper items',
      'hopper_remove_from_hopper' => 'Remove from Hopper',
    '1' => 'Standard Ingest Steps',
      'delete_update_copy_ingest' => ' - One step (delete, update, copy, ingest)',
      'update' => ' - Update batch records (no overwrite if islandora object exists)',
      'updateforce' => ' - Update batch records (force overwrite)',
      'checksize' => ' - Check file size of ingest files',
      'do_copy' => ' - Copy files from incoming to ingest location',
      'ingest_prepare' => ' - Islandora Ingest Preprocess',
      'gen_OCR' => ' - Generate OCR',
      'do_publish' => ' - Publish objects to site',
    '2' => 'Special Functionality',
      'update_workflow_values' => ' - Override Workflow Values (type,site,collection)',
      'fix_page_sizes' => ' - Fix Page size RELS_INT',
      'make_tn' => ' - Make TN derivative',
      'make_tn_large' => ' - Make TN_LARGE derivative',
      'make_jpg' => ' - Make JPG derivative',
      'make_jp2' => ' - Make JP2 derivative',
      'make_page_pdfs' => ' - (Make page PDF derivatives)',
      'aggregate' => ' - Aggregate OCR and PDF, make TN_LARGE',
      'set_status' => ' - Set Status',
      'solr_reindex' => ' - Solr reindex objects',
    '3' => 'Info / Cleanup',
      'checkobjects' => ' - Check for Islandora Objects',
      'links_to_objects' => ' - Admin links to objects',
      'deep_info_on_objects' => ' - Deep info on objects',
      'check_batch' => ' - Check batch for completedness',
      'get_common_batchvalues' => ' - Get batch values (site, collection, type)',
      'get_wf_paths' => ' - Get Bigfoot paths',
      'get_wf_barcodes_from_batch' => ' - Get barcodes from legacy batches',
      'query_solr' => ' - Query Solr for common values',
      'purge_OCR_PDF' => ' - Purge page OCR and PDF derivative',
      'delete_batch' => ' - DELETE islandora_batch_* records',
      'delete_batch_tempfiles' => ' - DELETE temp batch_* ingest folders',
      'purge_objects' => ' - DELETE objects',
      'pid_to_barcode' => ' - Convert PIDs to barcodes',
      'barcode_to_pid' => ' - Convert barcodes to PIDs',
  );

  // The easiest way to remove some of the options is to unset them if not $admin.
  if (!$admin) {
    $admin_only_process_barcode_modes = explode("\n", variable_get('upitt_workflow_admin_only_process_barcode_modes', 'purge_objects'));
    foreach ($admin_only_process_barcode_modes as $mode) {
      unset($process_options[$mode]);
    }
  }

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
      '#description' => 'These processes are not sequential.  Depending on what needs to be done, this form ' .
        'allows for processing a set of barcodes for a specific processing mode operation.  In the case of "Make TN_LARGE ' .
        'derivative", this <b>does not</b> need to be done with most models, but was added for a special case.',
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
  $mode = $form_state['values']['mode'];
  if ($mode == '0' || $mode == '1' || $mode == '2' || $mode == '3') {
    drupal_set_message('Invalid <b>Processing mode</b> selection.');
    return;
  }

  switch ($btn_id) {
    case 'edit-submit':
      if ($mode == 'update' || $mode == 'updateforce') {
        $markup = upitt_workflow_make_batch_records_for_barcodes($form_state['values']['barcodes'], $mode);
      } elseif ($mode == 'checksize') {
        $markup = upitt_workflow_do_copy_for_barcodes($form_state['values']['barcodes'], TRUE, TRUE);
      } else {
        $fn = 'upitt_workflow_' . $mode . '_for_barcodes';
        $markup = $fn($form_state['values']['barcodes']);
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
function upitt_workflow_checkobjects_for_barcodes($barcodes) {
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $markup = '<div class="text-report"><h3>Found in Islandora</h3><div class="found">' . implode("<br>", $check_results['found_pid_links']) . '</div><hr>' .
            '<h3>NOT found in Islandora</h3><div class="not-found">' . implode("<br>", $check_results['not_found_pids']) . '</div></div>';
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
function upitt_workflow_check_islandora_for_barcodes(&$barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $barcodes = upitt_workflow_pid_to_barcode_for_barcodes($barcodes, TRUE);
  
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $found_pids = $found_pid_links = $found_pid_manage_links = $not_found_pids = array();
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
        $is_paged = upitt_workflow_is_paged_object($islandora_object);
        $pages = ($is_paged) ? islandora_paged_content_get_pages($islandora_object) : array();

        $regen_jp2_link = '';
        $page_count = count($pages);
        $first_page = array_pop($pages);
        if ($first_page) {
          $page_object = islandora_object_load($first_page['pid']);
          $page_jp2_dimensions = upitt_workflow_get_jp2_dimensions($page_object);
          if ((isset($first_page['width']) && $first_page['width'] <> $page_jp2_dimensions['width']) ||
              (isset($first_page['height']) && $first_page['height'] <> $page_jp2_dimensions['height'])) {
            @list($namespace, $barcode_of_object) = explode(":", $islandora_object->id);
            $link_options = array(
              'query' => array(
                'mode' => 'make_jp2',
                'barcodes' => $barcode_of_object,
              ),
              'attributes' => array(
                'target'=>'_blank',
                'class' => 'bad',
              ),
            );

            $regen_jp2_link  = '<br><b>' .
                l('Regenerate JP2 for ' . $islandora_object->id, '/workflow/workflowadmin/process_barcodes/', $link_options) .
                '</b><br>';
          }
        }

        $found_pid_manage_links[$barcode] = $pid . ': ' .
                ($page_count ? '(' . number_format($page_count) . ' pages) ' : '') .
                $regen_jp2_link .
                l('DELETE', 'islandora/object/' . $pid . '/delete', array('attributes' => array('title' => 'DELETE object', 'class' => 'bad'))) . ' | ' .
                l('Regen', 'islandora/object/' . $pid . '/regenerate', array('attributes' => array('title' => 'Regenerate all derivatives', 'class' => 'bad'))) . ' | ' .
                l('Manage', 'islandora/object/' . $pid . '/manage', array('attributes' => array('title' => 'Manage object'))) . ' | ' .
                l('Datastreams', 'islandora/object/' . $pid . '/manage/datastreams', array('attributes' => array('title' => 'Manage | Datastreams'))) . ' | ' .
                l('Workflow' , 'workflow/object/' . $barcode, array('attributes' => array('title' => 'View workflow record for this object'))) . ' | ' .
                l('Publish' , 'workflow/object/' . $barcode . '/do_publish', array('attributes' => array('title' => 'Publish object to sites and relate to collections'))) . ' | ' .
                l('"' . $islandora_object->label . '"', 'islandora/object/' . $pid, array('attributes' => array('title' => 'Navigate to "' . htmlspecialchars($islandora_object->label) . '"')));
        $found_pid_links[$barcode] = '"' . $pid . '": ' .
                l($islandora_object->label, 'islandora/object/' . $pid);
        $found_pids[$barcode] = $pid;
      }
    }
  }
  return array('barcodes_arr' => $barcodes_arr,
      'found_pids' => $found_pids,
      'found_pid_links' => $found_pid_links,
      'found_pid_manage_links' => $found_pid_manage_links,
      'not_found_pids' => $not_found_pids);
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
function upitt_workflow_make_batch_records_for_barcodes($barcodes, $mode) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  global $user;

  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $markup = '<div class="text-report">';
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

  $markup .= '<h2>Found in Islandora</h2><pre class="good">' . implode("\n", $check_results['found_pids']) . '</pre>' .
            '<h2>NOT found in Islandora</h2><pre class="bad">' . implode("\n", $check_results['not_found_pids']) . '</pre>';

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $toggle = FALSE;
  $overwrite_if_exists = (($mode == 'updateforce') ? 1 : 0);
  foreach ($use_pids as $barcode => $pid) {
    $toggle = !$toggle;
    $markup .= "<div style='border-bottom: 1px solid black; padding-bottom: 8px; background-color:#" . ($toggle ? 'FFF' : 'F3F3F3') . "'>";
    $markup .= "  <br>barcode '" . $barcode . "' = islandora PID '" . $pid . "'<br>";
    $type = upitt_workflow_wf_item_get_type($barcode, $link);
    $model = upitt_workflow_wf_type_to_fedora_model($type);
    $batch_external_id_and_description = upitt_workflow_wf_item_get_name_and_desc($barcode, $link);
    $batch_host_path = upitt_workflow_batch_file_path($barcode, $link);
    $batch_host_path = str_replace("/usr/local/", "/ingest/", $batch_host_path);
    $description = $barcode . ' from legacy batch named `' . $batch_external_id_and_description['name'] . '` ' . $batch_external_id_and_description['description'];
    $collections = upitt_workflow_wf_item_get_collection($barcode, $link);
    $sites = upitt_workflow_wf_item_get_site($barcode, $link);
    $markup .= 'type = <b>' . $type . "</b><br>";
    $markup .= 'description = <b>' . $description . "</b><br>";
    $markup .= 'collections = <b>' . $collections . "</b><br>";
    $markup .= 'sites = <b>' . $sites . "</b><br>";
    $markup .= "</div>";
    $batch_record = upitt_workflow_batch_load_by_batch_external_id($barcode);
    if (array_key_exists('batch_external_id', $batch_record)) {
      $sql = "UPDATE `batch` SET " .
             // Only update if there was a value
             (($collections) ? "mapto_collections = '" . mysqli_real_escape_string($link, $collections) . "', " : '') .
             "batch_description = '" . mysqli_real_escape_string($link, $description) . "', " .
             // Only update if there was a value
             (($model) ? "default_type_of_resource, '" . mysqli_real_escape_string($link, $model) . "', " : '') .
             // Only update if there was a value
             (($sites) ? "mapto_site_id_values, '" . mysqli_real_escape_string($link, $sites) . "', " : '') .
             "overwrite_if_exists = " . $overwrite_if_exists . ", " .
             "batch_host = '" . (($batch_host_path) ? $batch_host_path : variable_get('upitt_workflow_ingest_incoming', '/ingest/incoming')) . "'," .
             "user = " . $user->uid;
    }
    else {
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
             "'" . (($batch_host_path) ? $batch_host_path : variable_get('upitt_workflow_ingest_incoming', '/ingest/incoming')) . "'," .
             $user->uid . ");";
    }
    mysqli_query($link, $sql);

    $batch_id = upitt_workflow_get_batch_id($barcode, FALSE);
    $item_id = upitt_workflow_insert_item_for_batch_external_id($barcode, $link);
    if (!$batch_id) {
      $batch_id = upitt_workflow_get_batch_id($barcode, TRUE);
      $sql = "REPLACE INTO `batch` (batch_external_id, " .
             (($collections) ? "mapto_collections, " : '') .
             "batch_description, " .
             (($model) ? "default_type_of_resource, " : '') .
             (($sites) ? "mapto_site_id_values, " : '') .
             "overwrite_if_exists, " .
             "batch_host, " .
             "user) VALUES (" .
             "'" . mysqli_real_escape_string($link, $barcode) . "', " .
             (($collections) ? "'" . mysqli_real_escape_string($link, $collections) . "', " : '') .
             "'" . mysqli_real_escape_string($link, $description) . "', " .
             (($model) ? "'" . mysqli_real_escape_string($link, $model) . "', " : '') .
             (($sites) ? "'" . mysqli_real_escape_string($link, $sites) . "', " : '') .
             $overwrite_if_exists . ", " .
             "'" . (($batch_host_path) ? $batch_host_path : variable_get('upitt_workflow_ingest_incoming', '/ingest/incoming')) . "'," .
             $user->uid . ");";
      mysqli_query($link, $sql);
    }
    upitt_workflow_relate_batch_to_item($batch_id, $item_id, $link);
  }

  $markup .= '</div>';
  mysqli_close($link);
  return $markup;
}

/** This will call the functions to delete, update, copy, and ingest barcodes in sequence.
 * 
 * @param type $barcodes
 */
function upitt_workflow_delete_update_copy_ingest_for_barcodes($barcodes) {
  $checksize_markup = upitt_workflow_do_copy_for_barcodes($barcodes, TRUE, TRUE);
  return (strstr($checksize_markup, 'Not enough space') == '') ?
    $checksize_markup .
    upitt_workflow_delete_batch_for_barcodes($barcodes) .
    upitt_workflow_purge_objects_for_barcodes($barcodes) .
    upitt_workflow_make_batch_records_for_barcodes($barcodes, 'update') .
    upitt_workflow_do_copy_for_barcodes($barcodes) .
    upitt_workflow_ingest_prepare_for_barcodes($barcodes) :
    $checksize_markup;
}

/** Delete all of the temp batch_* folders per barcode.
 *
 * @param string $barcodes
 */
function upitt_workflow_delete_batch_tempfiles_for_barcodes($barcodes) {
  $barcodes_arr = explode("\n", $barcodes);
  $v = file_directory_temp();
  $markup = '<div class="text-report">';
  foreach ($barcodes_arr as $barcode) {
    $folder = $v . '/batch_' . trim($barcode);
    if (file_exists($folder)) {
      $rm_temp_command = 'rm -rf ' . $folder;
      $output = array();
      exec($rm_temp_command, $output, $ret);
      if (!$ret) {
        $markup .= '<div class="bad">' . $folder . ' could not be deleted</div>';
      }
      else {
        $markup .= '<div class="good">' . $folder . ' deleted</div>';
      }
    }
    else {
      $markup .= '<div class="not-found">' . $folder . ' did not exist</div>';
    }
  }
  $markup .= '</div>';
  return $markup;
}



/** Delete all of those pesky islandora_batch_* records related to a set of PID
 * values.
 *
 * @param string $barcodes
 */
function upitt_workflow_delete_batch_for_barcodes($barcodes) {
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $sql_sets = "select q.sid, q.id from islandora_batch_queue q join islandora_batch_set s ON (s.id = q.sid) where q.id like '" . $pid . "%'";
    $result = db_query($sql_sets);
    $record = $result->fetchAssoc();
    $result_set = $result_report_sets_processed = $result_queue = $result_queue_messages = $result_report_objects_processed = $result_resources = $result_state = 0;
    if (isset($record['sid'])) {
      $sid = $record['sid'];
      $markup .= '<div>islandora_batch_set = ' . $sid . "</div>";
      $result_set = db_delete('islandora_batch_set')->condition('id', $sid)->execute();
      $result_report_sets_processed = db_delete('islandora_batch_report_sets_processed')->condition('sid', $sid)->execute();
      $result_queue = db_delete('islandora_batch_queue')->condition('sid', $sid)->execute();
    }
    if (isset($record['id'])) {
      $result_queue_messages = db_delete('islandora_batch_queue_messages')->condition('id', '%' . db_like($pid) . '%', 'LIKE')->execute();
      $result_report_objects_processed = db_delete('islandora_batch_report_objects_processed')->condition('pid', '%' . db_like($pid) . '%', 'LIKE')->execute();
      $result_resources = db_delete('islandora_batch_resources')->condition('id', '%' . db_like($pid) . '%', 'LIKE')->execute();
      $result_state = db_delete('islandora_batch_state')->condition('id', '%' . db_like($pid) . '%', 'LIKE')->execute();
    }
    $sql = "delete from islandora_batch_queue where id like '" . $pid . "%'";
    $result = db_query($sql);
    $sql = "delete from islandora_batch_report_objects_processed where id like '" . $pid . "%'";
    $result = db_query($sql);
    $sql = "delete from islandora_batch_queue_messages where id like '" . $pid . "%'";
    $result = db_query($sql);

    $total = $result_set + $result_report_sets_processed + $result_queue + $result_queue_messages + $result_report_objects_processed + $result_resources + $result_state;
    $class = ($total) ? 'good' : 'bad';
    $markup .= '<div class="' . $class . '"><b>records deleted for ' . $pid . '</b><pre>' .
            'islandora_batch_set = ' . $result_set . "<br>" .
            'islandora_batch_report_sets_processed = ' . $result_report_sets_processed . "<br>" .
            'islandora_batch_queue = ' . $result_queue . "<br>" .
            'islandora_batch_queue_messages = ' . $result_queue_messages . "<br>" .
            'islandora_batch_report_objects_processed = ' . $result_report_objects_processed . "<br>" .
            'islandora_batch_resources = ' . $result_resources . "<br>" .
            'islandora_batch_state = ' . $result_state . "<br>" .
            "</pre></div>";
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_purge_OCR_PDF_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);
    if (is_object($islandora_object)) {
      $is_paged = upitt_workflow_is_paged_object($islandora_object);
      $pages = ($is_paged) ? islandora_paged_content_get_pages($islandora_object) : array();
      upitt_workflow_purge_childpages_datastreams($islandora_object, $pages, 'OCR');
      upitt_workflow_purge_childpages_datastreams($islandora_object, $pages, 'PDF');
      $markup .= '<div class="good">Successfully purged OCR or PDF datastream for ' . $pid . '.</div>';
    }
    else {
      if ($barcode) {
        $missing_barcodes[] = $barcode;
      }
    }
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_make_page_pdf_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);
    if (is_object($islandora_object)) {
      $is_paged = upitt_workflow_is_paged_object($islandora_object);
      $pages = ($is_paged) ? islandora_paged_content_get_pages($islandora_object) : array();
      foreach ($pages as $child_page) {
        if (isset($child_page['pid'])) {
          $child_page_obj = islandora_object_load($child_page['pid']);
          $obj_ds = $child_page_obj['OBJ'];
          if (!upitt_workflow_make_page_pdf($child_page_obj)) {
            $markup .= '<div class="bad">Failed to created PDF for ' . $child_page['pid'] . '.</div>';
          }
          else {
            $markup .= '<div class="good">Successfully created PDF for ' . $child_page['pid'] . '.</div>';
          }
        }
      }
    } 
    else {
      if ($barcode) {
        $missing_barcodes[] = $barcode;
      }
    }
    $markup .= '<div class="good"><b>PDF derivatives created for the pages of ' . $pid . "</b></div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_make_jpg_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $markup .= "<b>Start " . date('Y-m-d H:i:s') . "</b><br>";
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);
    $is_paged = upitt_workflow_is_paged_object($islandora_object);
    if ($is_paged) {
      $pages = ($is_paged) ? islandora_paged_content_get_pages($islandora_object) : array();
      foreach ($pages as $child_page) {
        if (isset($child_page['pid'])) {
          $child_page_obj = islandora_object_load($child_page['pid']);
          if (is_object($child_page_obj)) {
            if (!upitt_workflow_make_jpg($child_page_obj, $child_page_obj)) {
              $missing_barcodes[] = $child_page['pid'];
            }
          }
        }
      }
    }
    elseif (is_object($islandora_object)) {
      if (!upitt_workflow_make_jpg($islandora_object, $islandora_object)) {
        $missing_barcodes[] = $barcode;
      }
    } else {
      $missing_barcodes[] = $barcode;
    }
    $markup .= '<div>JPG created for ' . $pid . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes or their OBJ datastream could not be found  ' . implode(", ", $missing_barcodes), 'warning');
  }

  $markup .= "<b>Done " . date('Y-m-d H:i:s') . "</b><br>";
  $markup .= "<b>" . count($barcodes_arr) . " objects processed</b><br>";
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_make_jp2_for_barcodes($barcodes) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $markup .= "<b>Start " . date('Y-m-d H:i:s') . "</b><br>";
  $ingest_namespace = upitt_workflow_get_ingest_namespace();

  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);

    if (is_object($islandora_object)) {
      $is_paged = upitt_workflow_is_paged_object($islandora_object);
      if ($is_paged) {
        $pages = array_keys(islandora_paged_content_get_pages($islandora_object));
        foreach ($pages as $page) {
          # Get the PageObject.
          $pageObject = islandora_object_load($page);
          error_log('Start updating page size for page ' . $page);

          if (!upitt_workflow_make_jp2($pageObject)) {
            $missing_barcodes[] = $barcode;
          }
        }
      }
      else {
        error_log('Start updating page size for book ' . $pid);
        if (!upitt_workflow_make_jp2($islandora_object)) {
          $missing_barcodes[] = $barcode;
        }
      }
    }
    else {
      $missing_barcodes[] = $barcode;
    }
    $markup .= '<div>JP2 created for ' . $pid . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes or their OBJ datastream could not be found  ' . implode(", ", $missing_barcodes), 'warning');
  }
  
  $markup .= "<b>Done " . date('Y-m-d H:i:s') . "</b><br>";
  $markup .= "<b>" . count($barcodes_arr) . " objects processed</b><br>";
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_fix_page_sizes_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);

    $pages = array_keys(islandora_paged_content_get_pages($islandora_object));
    $page_count = count($pages);

    error_log('Start updating page size for book ' . $pid);
    // For each PageObject update the height/width of the image in RELS-INT.
    foreach ($pages as $page) {
      # Working on this PID.
      error_log("PID: $page");

      # Get the PageObject.
      $pageObject = islandora_object_load($page);

      if (is_object($pageObject)) {
        if (!$changed_radius_calc = upitt_workflow_fix_page_size($pageObject)) {
          $missing_barcodes[] = $barcode;
        }
        else {
          if ($changed_radius_calc > 1) {
            $markup .= '<div>Page size RELS-INT adjusted for ' . $page . "</div>";
          }
          else {
            $markup .= '<div>Page size checked for ' . $page . "</div>";
          }
        }
      }
      else {
        if ($barcode) {
          $missing_barcodes[] = $barcode;
        }
      }
    }
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_fix_page_size($pageObject) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  # Get the RELS-INT size of the image.
  # This should probably be the OBJ datastream but in some cases
  # the OBJ datastream doesn't work.
  $jp2_datastream = $pageObject['JP2'];
  $rels = $jp2_datastream->relationships;
  $width_rels_in = $rels->get(ISLANDORA_RELS_INT_URI, 'width');
  $width_rels = $width_rels_in[0]['object']['value'];
  $height_rels_in = $rels->get(ISLANDORA_RELS_INT_URI, 'height');
  $height_rels = $height_rels_in[0]['object']['value'];

  # Get the actual image dimensions.
  $DSID = "OBJ";
  $obj_datastream = $pageObject['OBJ'];
  $mimetype = $obj_datastream->mimetype;
  $fileextension = upitt_workflow_getMimetypeToFileExtension($mimetype);
  $filename = tempnam("temporary://", "$DSID.$fileextension");
  $retval = upitt_workflow_getDatastreamToFile($pageObject,"OBJ","$filename");
  @list($width, $height) = getimagesize($filename);
  @unlink($filename);

  // REMOVE any RELS_EXT width/height - that value would be wrong!
  $rels->remove(ISLANDORA_RELS_EXT_URI, 'width', NULL);
  $rels->remove(ISLANDORA_RELS_EXT_URI, 'height', NULL);

  upitt_workflow_set_relationship($rels, ISLANDORA_RELS_INT_URI, 'width', $width, 1);
  upitt_workflow_set_relationship($rels, ISLANDORA_RELS_INT_URI, 'height', $height, 1);

  return sqrt(($width - $width_rels) * ($width - $width_rels) + ($height == $height_rels) * ($height == $height_rels));
  # Report on the differences.
  error_log("Actual Width: $width, Height: $height,  \n" .
            "RELS-INT Width: $width_rels, Height: $height_rels");
}

function upitt_workflow_make_tn_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  $barcodes = upitt_workflow_pid_to_barcode_for_barcodes($barcodes, TRUE);

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);
    if (is_object($islandora_object)) {
      if (!upitt_workflow_make_tn($islandora_object, $islandora_object)) {
        $missing_barcodes[] = $barcode;
      }
    }
    else {
      if ($barcode) {
        $missing_barcodes[] = $barcode;
      }
    }
    $markup .= '<div>TN created for ' . $pid . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_make_tn_large_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  $barcodes = upitt_workflow_pid_to_barcode_for_barcodes($barcodes, TRUE);

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);
    if (is_object($islandora_object)) {
      if (!upitt_workflow_make_tn_large($islandora_object, $islandora_object)) {
        $missing_barcodes[] = $barcode;
      }
      else {
        $markup .= '<div>TN_LARGE created for ' . $pid . "</div>";
      }
    }
    else {
      if ($barcode) {
        $missing_barcodes[] = $barcode;
      }
    }
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some objects could not be found or their TN_LARGE could not be created ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_set_status_for_barcodes($barcodes) {
  $array = explode("\r\n", $barcodes);
  $markup = '';
  if (count($array) > 1) {
    $status_line = array_shift($array);
    if (!strstr($status_line, "=")) {
      $markup .= '<h3 class="bad">Status line is not correct</h3><p class="bad">The first line in a set of barcodes for "set status" needs to be something like:<br><code class="good">status=145</code></p>';
    }
    else {
      @list($junk, $status_id) = explode("=", $status_line, 2);
      $status_id = trim($status_id);
      if (!is_numeric($status_id)) {
        $markup .= '<h3 class="bad">Status line is not correct</h3><div class="bad">The status line does not contain a value that is numeric.  Please provide a value like:<br><code class="good">status=145</code></div>';
      }
      else {
        $markup .= "<h1>Set status to " . $status_id . "</h1>";
        $barcodes = implode("\r\n", $array);
        $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
        $ingest_namespace = upitt_workflow_get_ingest_namespace();
        foreach ($check_results['found_pids'] as $key => $pid) {
          if ($pid) {
            $barcode = str_replace($ingest_namespace, "", $pid);
            if (upitt_workflow_set_status_for_batch($barcode, $status_id, NULL, FALSE)) {
              $markup .= '<div class="good">Status for ' . $pid . ' set to ' . $status_id . '.</div>';
            }
            else {
              $markup .= '<div class="bad">Status for ' . $pid . ' could not be set to ' . $status_id . '.</div>';
            }
          }
        }
      }
    }
  }
  else {
    $markup .= '<p class="bad">For a complete list of available status values, use the corresponding `id` value from this page: ' . l('Transaction Actions', '/workflow/workflowadmin/manage/transaction_actions') . '</p>';
  }
  return $markup;
}

function upitt_workflow_datastream_results($islandora_object, $ingest_namespace, $is_child = FALSE, $is_paged = FALSE, $model_requirements = array()) {
  $barcode = str_replace($ingest_namespace, "", $islandora_object->id);
  $markup = '<div class="good">' . $islandora_object->id . ': ' .
    l('DELETE', 'islandora/object/' . $islandora_object->id . '/delete', array('attributes' => array('title' => 'DELETE object', 'class' => 'bad'))) . ' | ' .
    l('Regen', 'islandora/object/' . $islandora_object->id . '/regenerate', array('attributes' => array('title' => 'Regenerate all derivatives', 'class' => 'bad'))) . ' | ' .
    l('Manage', 'islandora/object/' . $islandora_object->id . '/manage', array('attributes' => array('title' => 'Manage object'))) . ' | ' .
    l('Datastreams', 'islandora/object/' . $islandora_object->id . '/manage/datastreams', array('attributes' => array('title' => 'Manage | Datastreams'))) . ' | ' .
    l('Workflow' , 'workflow/object/' . $barcode, array('attributes' => array('title' => 'View workflow record for this object'))) . ' | ' .
    ($is_child ? '' : l('Publish' , 'workflow/object/' . $barcode . '/do_publish', array('attributes' => array('title' => 'Publish object to sites and relate to collections'))) . ' | ') .
    (($is_child || !$is_paged) ? '' : '<a href="/workflow/workflowadmin/process_barcodes/?barcodes=' . $barcode . '&mode=aggregate">Aggregate OCR PDF</a> | ') .
    l('"' . $islandora_object->label . '"', 'islandora/object/' . $islandora_object->id) .
    '</div><div class="datastreams">';
  $ds_arr = array();
  $pdf_size = FALSE;
  $has_OCR = $has_PDF = $has_TN_LARGE = FALSE;
  $size_OCR = 0;
  $required_datastreams = array();
  foreach ($model_requirements as $model => $datastream_requirements) {
    foreach ($datastream_requirements as $dsid => $requirements) {
      if (!$requirements['optional']) {
        if (array_search($dsid, $required_datastreams) === FALSE) {
          $required_datastreams[] = $dsid;
        }
      }
    }
  }
  foreach ($islandora_object as $ds) {
    $ds_arr[$ds->id]['id'] = $ds->id;
    if ($ds->id == 'OCR') {
      $has_OCR = TRUE;
      $size_OCR = $ds->size;
    }
    if ($ds->id == 'TN_LARGE') {
      $has_TN_LARGE = TRUE;
    }
    if ($ds->id == 'PDF') {
      $has_PDF = TRUE;
    }
    if ($ds->id == 'PDF') {
      $pdf_size = $ds->size;
    }
  }
  ksort($ds_arr);
  $ds_ids = array();
  foreach ($ds_arr as $dsid => $ds) {
    $ds_ids[] = $ds['id'];
  }
  foreach ($required_datastreams as $required_datastream_id) {
    if (array_search($required_datastream_id, $ds_ids) === FALSE) {
      $ds_ids[] = '<span class="bad" title="required ' . $required_datastream_id . ' datastream missing">' . $required_datastream_id . '</span>';
    }
  }
  return array('markup' => $markup . implode(", ", $ds_ids) . '</div>', 'OCR_size' => $size_OCR, 'pdf_size' => $pdf_size);
}

function upitt_workflow_child_object_results($object, $ingest_namespace) {
  // if paged object, then use the pages function to get the child page objects
  $markup = $children_named = '';
  $child_count = $potential_child_count = 0;
  if (!(array_search('islandora:compoundCModel', $object->models) === FALSE)) {
    module_load_include('module', 'islandora_compound_object', 'islandora_compound_object');
    $child_pids = islandora_compound_object_get_parts($object->id);
    // Put the results in the same structure that is expected back by the
    // solr_record method below.
    $object_members = array();
    foreach ($child_pids as $idx => $pid) {
      $object_members[$idx] = array('PID' => $pid);
      $child_count++;
    }
    $children_named = ($child_count ? 'Compound Object members' : 'no Compound Object members');
  }
  elseif (!(array_search('islandora:findingAidCModel', $object->models) === FALSE)) {
    // get Finding Aids members.
    module_load_include('module','upitt_islandora_findingaids_relationships');
    $related_objects = findingaid_objects($object->id);
    // get all the objects that are currently related to this finding aid object.
    $ead_referenced_objects = findingaid_EAD_objects($object);
    $referenced_objects = array_merge($ead_referenced_objects, $related_objects);
    $related_count = 0;
    $existing_related_members = $pending_online_related_members = $barcodes = array();
    foreach ($referenced_objects as $child_object_pid => $child_rec_array) {
      $child_object = islandora_object_load($child_object_pid);
      $related = FALSE;
      $exists = is_object($child_object);
      if ($exists) {
        $object_members[] = array('PID' => $child_object_pid);
        $existing_related_members[] = l($child_object_pid . ': "' . $child_object->label . '"', "islandora/object/{$child_object_pid}");
        $child_count++;
      }
      else {
        $pending_online_related_members[] = $child_object_pid;
        @list($namespace, $barcode) = explode(":", $child_object_pid);
        $barcodes[] = $barcode;
        $potential_child_count++;
      }
      $children_named = ($child_count ? 'Finding Aid members' : 'no Finding Aid members') .
        ($potential_child_count ? ' (found ' . $potential_child_count . ' unrelated Finding Aid members)' : '');
    }
    $link_options = array();
    if (count($pending_online_related_members) > 0) {
      $link_options = array(
        'query' => array(
          'mode' => 'update',
          'barcodes' => implode(",",$barcodes),
        ),
        'attributes' => array('target'=>'_blank'),
      );
    }

    $markup = '<div><pre>' . implode(', ', $existing_related_members) . '</pre>' .
      '<h4>Pending online members</h4><pre>' .
          ((count($pending_online_related_members) > 0) ?
            '<b>' . l('Process these objects', '/workflow/workflowadmin/process_barcodes/', $link_options) . '</b> ' . implode(', ', $pending_online_related_members) :
            '<i>no pending object members</i>') . '</pre></div>';
  }
  else {
    $object_members = array();
    $is_paged = upitt_workflow_is_paged_object($object);
    if ($is_paged) {
      $child_pages = islandora_paged_content_get_pages_ri($object);
      // This call will set 'pid' elements, previous code sets 'PID', so need to
      // swap them over
      foreach ($child_pages as $child_page) {
        if (isset($child_page['pid'])) {
          $object_members[] = array('PID' => $child_page['pid']);
          $child_count++;
        }
      }
      $children_named = ($child_count ? 'Pages' : 'no Pages');
    }
  }
  $total_OCR_size = 0;
  $all_childs_had_OCR = $all_childs_had_PDF = TRUE;
  foreach ($object_members as $member_solr_doc) {
    $member_object = islandora_object_load($member_solr_doc['PID']);
    if (is_object($member_object)) {
      $all_childs_had_OCR &= isset($member_object['OCR']);
      $all_childs_had_PDF &= isset($member_object['PDF']);
      $results = upitt_workflow_datastream_results($member_object, $ingest_namespace, TRUE);
      $total_OCR_size += $results['OCR_size'];
      $markup .= $results['markup'] . "<br>";
    }
    else {
      $markup .= '<i class="bad">' . $member_solr_doc['PID'] . ' could not load</i><hr>';
    }
  }
  return array(
      'markup' => ($markup ? '<br><div class="pl_20"><b>object members</b><br>' . $markup . '</div>' : '<i class="bad">no object members</i><br>'),
      'total_OCR_size' => $total_OCR_size,
      'all_childs_had_OCR' => ($all_childs_had_PDF ? TRUE : FALSE),
      'all_childs_had_PDF' => ($all_childs_had_PDF ? TRUE : FALSE),
      'child_count' => $child_count,
      'children_named' => $children_named,
  );
}

/**
 * Inspect the islandora_object's models -- and check the $model_requirements array 
 * for an existing value for this model -- and add the datastream requirements if it 
 * did not exist.
 *
 * This will return the model_pids from the islandora_object that have datastream requirements.
 */
function upitt_workflow_object_model_datastream_requirements($islandora_object, &$model_requirements) {
  $models = array();
  $model_pids = array();
  if (is_array($islandora_object->models)) {
    foreach ($islandora_object->models as $model_pid) {
      if ($model_pid <> 'fedora-system:FedoraObject-3.0') {
        if (array_key_exists($model_pid, $model_requirements) === FALSE) {
          $model = @islandora_object_load($model_pid);
          if (isset($model) && $model) {
            $model_pids[] = $model_pid;
            $ds_reqs = islandora_get_datastreams_requirements_from_content_model($model);
            if (array_key_exists('RELS-EXT', $ds_reqs)) {
              if ($ds_reqs['RELS-EXT']['optional']) {
                $ds_reqs['RELS-EXT']['optional'] = FALSE;
              }
              if ($ds_reqs['TN']['optional']) {
                $ds_reqs['TN']['optional'] = FALSE;
              }
            }
            // We want to override the value for RELS-INT here - it's required.
            if (is_array($ds_reqs)) {
              $model_requirements[$model_pid] = $ds_reqs;
            }
          }
        }
        else {
          // the model already has been populated for this model_pid, still must add this model_pid to the models for this object.
          $model_pids[] = $model_pid;
        }
      }
    }
  }
  // RELS-EXT
  return $model_pids;
}

function upitt_workflow_deep_info_on_objects_for_barcodes($barcodes) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  $markup = '<div class="text-report"><h3>Deep info on objects</h3>';
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $ingest_namespace = upitt_workflow_get_ingest_namespace();

  // Set up arrays for keeping track of what needs done with the parent objects.
  $need_ocr_pdf = $need_aggregated = $done_ocr_aggregation = array();
  foreach ($check_results['found_pids'] as $barcode => $pid) {
    $islandora_object = islandora_object_load($pid);
    $ocr_aggregate_size = (isset($islandora_object['OCR'])) ? $islandora_object['OCR']->size : 0;
    $model_requirements = $disds = array();
    $model_pids = upitt_workflow_object_model_datastream_requirements($islandora_object, $model_requirements);
    foreach ($model_pids as $model_pid) {
      $ds_reqs_output = array();
      foreach ($model_requirements[$model_pid] as $ds_key => $ds_requirement) {
        $ds_reqs_output[] = '<span' . ((isset($ds_requirement['optional']) && $ds_requirement['optional'] == 1) ? ' class="optional"' : '') . '>' . $ds_key . '</span>';
      }
      $dsids[$model_pid] = implode(", ", $ds_reqs_output);
    }
    if (count($dsids) > 0) {
      $markup .= '<div><b>' . implode(", ", (array)$islandora_object->models) . '  datastream requirements</b><br>' . implode(", ", $dsids) . '</div>';
    }
    $is_paged = upitt_workflow_is_paged_object($islandora_object);
    $datastream_results = upitt_workflow_datastream_results($islandora_object, $ingest_namespace, FALSE, $is_paged, $model_requirements);
    $child_results = upitt_workflow_child_object_results($islandora_object, $ingest_namespace);
    if ($is_paged) {
      $parent_had_OCR_aggregate = (isset($islandora_object['OCR']) ? 1 : 0);
      $parent_had_PDF_aggregate = (isset($islandora_object['PDF']) ? 1 : 0);
      if ($parent_had_OCR_aggregate && $parent_had_PDF_aggregate && !($child_results['all_childs_had_OCR']) && !($child_results['all_childs_had_PDF'])) {
        // OCR / PDF all done
        $done_ocr_aggregation[] = $barcode;
        $markup .= '<div class="good">OCR and PDF aggregation complete.</div>';
      } elseif ((!$parent_had_OCR_aggregate || !$parent_had_PDF_aggregate) && $child_results['all_childs_had_OCR'] && $child_results['all_childs_had_PDF']) {
        // Parent did not have either OCR or PDF aggregate, but children have all datastreams ready.
        $need_aggregated[] = $barcode;
        $markup .= '<div class="bad">Needs OCR/PDF aggregation.</div>';
      } elseif ((!$parent_had_OCR_aggregate || !$parent_had_PDF_aggregate) && !($child_results['all_childs_had_OCR'] || $child_results['all_childs_had_PDF'])) {
        // Parent did not have either OCR or PDF aggregate, and at least one child did not have all datastreams ready.
        $need_ocr_pdf[] = $barcode;
        $markup .= '<div class="bad">Needs OCR and/or PDF for child pages as well as OCR/PDF aggregation.</div>';
      } else {
        $markup .= '<div class="bad">Needs OCR and/or PDF for child pages OR simply OCR/PDF aggregation.</div>';
      }


      if ($datastream_results['pdf_size'] === FALSE) {
        $markup .= '<div class="bad">There is no PDF datastream.</div>';
      }
      elseif ($datastream_results['pdf_size'] < 2193) {
        $markup .= '<div class="bad">PDF file is less than 2,193 bytes -- size = ' . $datastream_results['pdf_size'] . '.</div>';
      }
      else {
        $markup .= '<div class="good">PDF file size is good - size = ' . $datastream_results['pdf_size'] . '.</div>';
      }
    }
    
    $markup .= $datastream_results['markup'];
    // check the total OCR size of child objects - if this is 0, skip this step because aggregate OCR already occurred.
    $ocr_size_ratio = (($ocr_aggregate_size) ? $child_results['total_OCR_size'] / $ocr_aggregate_size : FALSE);
    if ($child_results['total_OCR_size'] && $is_paged && $ocr_aggregate_size <> $child_results['total_OCR_size']) {
      $markup .= '<div class="bad">OCR aggregate size not equal to total size of all member OCR</div>';
      $markup .= '<div class="bad pl">OCR aggregate = ' . $ocr_aggregate_size . ', total size of all member OCR = ' . $child_results['total_OCR_size'] . '</div>';
    }
    $markup .= $child_results['markup'] . "<hr>";
  }

  // $do_ocr_link, $do_aggregation_link
  $do_ocr_link = (count($need_ocr_pdf) > 0) ? '<b><a href="/workflow/workflowadmin/process_barcodes/?barcodes=' . implode(",", $need_ocr_pdf) . '&mode=gen_OCR">Generate OCR / PDF for these</a></b><br>' : '';
  $do_aggregation_link = (count($need_aggregated) > 0) ? '<b><a href="/workflow/workflowadmin/process_barcodes/?barcodes=' . implode(",", $need_aggregated) . '&mode=aggregate">Aggregate these OCR PDF</a></b><br>' : '';
  $need_ocr_pdf = $need_aggregated = $done_ocr_aggregation = array();
  $markup .= '<h3>Found in Islandora</h3><pre class="good">' . implode("\n", $done_ocr_aggregation) . '</pre>';
  $markup .= '<h3>Need OCR aggregation</h3>' . $do_aggregation_link . '<pre class="bad">' . implode("\n", $need_aggregated) . '</pre>';
  $markup .= '<h3>Need OCR & PDF generated</h3>' . $do_ocr_link . '<pre class="bad">' . implode("\n", $need_ocr_pdf) . '</pre>';

  $markup .= '<h2>Found in Islandora</h2><pre class="good">' . implode("\n", $check_results['found_pid_manage_links']) . '</pre>' .
            '<h2>NOT found in Islandora</h2><pre class="bad">' . implode("\n", $check_results['not_found_pids']) . '</pre>';
  $markup .= '</div>';
  return $markup;
}

/**
 * This will determine whether each value is complete.
 *
 * This will take barcodes or batch and...
 *  1. Get the object's model -- and the requirements for that model type
 *      Check that the object (and its children) have the requirements.
 *
 *   2. See if the object is related to collection/s and site/s.
 *      Display the Publish link if it is not related to a site AND the batch
 *      record has a value for mapto_site_id_values.
 *
 * @param type $values
 * @return string
 */
function upitt_workflow_check_batch_for_barcodes($values) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  $markup = '<div class="text-report"><h3>Check barcodes / batch for completedness</h3>';

  $missing_barcodes = array();
  $barcode_batch_array = upitt_workflow_barcode_or_batch_barcodes($values);
  $last_batch_name = '';
  $barcodes = '';
  foreach ($barcode_batch_array as $barcode_batch) {
    $barcodes .= $barcode_batch['barcode'] . "\n";
  }
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $ingest_namespace = upitt_workflow_get_ingest_namespace();

  // Set up arrays for keeping track of what needs done with the parent objects.
  $need_ocr_pdf = $need_aggregated = $done_ocr_aggregation = array();
  foreach ($check_results['found_pids'] as $barcode => $pid) {
    // NEED TO USE THIS batch_record for objects that need to be published yet.
    $batch_record = upitt_workflow_batch_load_by_batch_external_id($barcode);

    $islandora_object = islandora_object_load($pid);
    $ocr_aggregate_size = (isset($islandora_object['OCR'])) ? $islandora_object['OCR']->size : 0;
    $model_requirements = $dsids = array();
    $model_pids = upitt_workflow_object_model_datastream_requirements($islandora_object, $model_requirements);

    $is_paged = upitt_workflow_is_paged_object($islandora_object);
    $datastream_results = upitt_workflow_datastream_results($islandora_object, $ingest_namespace, FALSE, $is_paged, $model_requirements);
    $child_results = upitt_workflow_child_object_results($islandora_object, $ingest_namespace);
    if ($is_paged) {
      $parent_had_OCR_aggregate = (isset($islandora_object['OCR']) ? 1 : 0);
      $parent_had_PDF_aggregate = (isset($islandora_object['PDF']) ? 1 : 0);
      if ($parent_had_OCR_aggregate && $parent_had_PDF_aggregate && !($child_results['all_childs_had_OCR']) && !($child_results['all_childs_had_PDF'])) {
        // OCR / PDF all done
        $done_ocr_aggregation[] = $barcode;
        $markup .= '<div class="good">OCR and PDF aggregation complete.</div>';
      } elseif ((!$parent_had_OCR_aggregate || !$parent_had_PDF_aggregate) && $child_results['all_childs_had_OCR'] && $child_results['all_childs_had_PDF']) {
        // Parent did not have either OCR or PDF aggregate, but children have all datastreams ready.
        $need_aggregated[] = $barcode;
        $markup .= '<div class="bad">Needs OCR/PDF aggregation.</div>';
      } elseif ((!$parent_had_OCR_aggregate || !$parent_had_PDF_aggregate) && !($child_results['all_childs_had_OCR'] || $child_results['all_childs_had_PDF'])) {
        // Parent did not have either OCR or PDF aggregate, and at least one child did not have all datastreams ready.
        $need_ocr_pdf[] = $barcode;
        $markup .= '<div class="bad">Needs OCR and/or PDF for child pages as well as OCR/PDF aggregation.</div>';
      } else {
        $markup .= '<div class="bad">Needs OCR and/or PDF for child pages OR simply OCR/PDF aggregation.</div>';
      }


      if ($datastream_results['pdf_size'] === FALSE) {
        $markup .= '<div class="bad">There is no PDF datastream.</div>';
      }
      elseif ($datastream_results['pdf_size'] < 2193) {
        $markup .= '<div class="bad">PDF file is less than 2,193 bytes -- size = ' . $datastream_results['pdf_size'] . '.</div>';
      }
      else {
        $markup .= '<div class="good">PDF file size is good - size = ' . $datastream_results['pdf_size'] . '.</div>';
      }
    }

    $markup .= $datastream_results['markup'];
    $markup .= upitt_workflow_compare_object_to_common_batch_values_markup($islandora_object, $batch_record);

    // check the total OCR size of child objects - if this is 0, skip this step because aggregate OCR already occurred.
    $ocr_size_ratio = (($ocr_aggregate_size) ? $child_results['total_OCR_size'] / $ocr_aggregate_size : FALSE);
    if ($child_results['total_OCR_size'] && $is_paged && $ocr_aggregate_size <> $child_results['total_OCR_size']) {
      $markup .= '<div class="bad">OCR aggregate size not equal to total size of all member OCR</div>';
      $markup .= '<div class="bad pl">OCR aggregate = ' . $ocr_aggregate_size . ', total size of all member OCR = ' . $child_results['total_OCR_size'] . '</div>';
    }
    
    $markup .= "<b>" . ($child_results['child_count'] ? $child_results['child_count'] : '') .
            " " . $child_results['children_named'] . "</b><hr>";
  }

  // $do_ocr_link, $do_aggregation_link
  $do_ocr_link = (count($need_ocr_pdf) > 0) ? '<b><a href="/workflow/workflowadmin/process_barcodes/?barcodes=' . implode(",", $need_ocr_pdf) . '&mode=gen_OCR">Generate OCR / PDF for these</a></b><br>' : '';
  $do_aggregation_link = (count($need_aggregated) > 0) ? '<b><a href="/workflow/workflowadmin/process_barcodes/?barcodes=' . implode(",", $need_aggregated) . '&mode=aggregate">Aggregate these OCR PDF</a></b><br>' : '';
  $need_ocr_pdf = $need_aggregated = $done_ocr_aggregation = array();
  $markup .= '<h3>Found in Islandora</h3><pre class="good">' . implode("\n", $done_ocr_aggregation) . '</pre>';
  $markup .= '<h3>Need OCR aggregation</h3>' . $do_aggregation_link . '<pre class="bad">' . implode("\n", $need_aggregated) . '</pre>';
  $markup .= '<h3>Need OCR & PDF generated</h3>' . $do_ocr_link . '<pre class="bad">' . implode("\n", $need_ocr_pdf) . '</pre>';

  $markup .= '<h2>Found in Islandora</h2><pre class="good">' . implode("\n", $check_results['found_pid_manage_links']) . '</pre>' .
            '<h2>NOT found in Islandora</h2><pre class="bad">' . implode("\n", $check_results['not_found_pids']) . '</pre>';
  $markup .= '</div>';
  return $markup;
}

/**
 * This will fetch the site, collections, and type from the batch table.
 *
 * For the provided $barcodes value, each line could be a batch or a barcode. 
 *
 * @param type $barcodes
 * @return string
 *   HTML markup of the results for this request.
 */
function upitt_workflow_get_common_batchvalues_for_barcodes($values) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $missing_barcodes = array();
  $barcode_batch_array = upitt_workflow_barcode_or_batch_barcodes($values);

  $markup = '<div class="text-report">';
  $last_batch_name = '';
  foreach ($barcode_batch_array as $barcode_batch) {
    // 1. load the batch record for the given barcode - and pass to the function that does the copy operation
    $batch_record = upitt_workflow_batch_load_by_batch_external_id($barcode_batch['barcode']);
    if (count($batch_record) < 1 && $barcode_batch['barcode']) {
      $missing_barcodes[] = $barcode_batch['barcode'];
    }
    else {
      if (($barcode_batch['batch'] <> $last_batch_name) && ($barcode_batch['batch'] <> $barcode_batch['barcode'])) {
        $markup .= '<pre><b>' . $barcode_batch['batch'] . '</b></pre>';
      }
      elseif ($barcode_batch['batch'] <> $last_batch_name) {
        $markup .= "<br />";
      }
      $markup .= upitt_workflow_common_batch_values_markup($barcode_batch['barcode'], $batch_record);
      $last_batch_name = $barcode_batch['batch'];
    }
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_common_batch_values_markup($prefix = '', $batch_record) {
  if (array_key_exists('default_type_of_resource', $batch_record)) {
    return '<div>' . (($prefix) ? $prefix . ': ' : '') . "type = '" .
              $batch_record['default_type_of_resource'] . "', " .
              "site/s = '" . $batch_record['mapto_site_id_values'] . "', collection/s = '" . $batch_record['mapto_collections'] . "'</div>";
  }
  else {
    return '<i>Batch record missing</i><br>';
  }
}

function upitt_workflow_compare_object_to_common_batch_values_markup($islandora_object, $batch_record) {
  $model_class = 'bad';
  if (array_key_exists('default_type_of_resource', $batch_record)) {
    // Compare the Islandora Object's to what is in the batch_record:
    if (is_array($islandora_object->models)) {
      foreach ($islandora_object->models as $model_pid) {
        if ($model_pid <> 'fedora-system:FedoraObject-3.0') {
          @list($model_namespace, $model_pid_only) = explode(":", $model_pid);
          $model_class = ($model_pid_only == $batch_record['default_type_of_resource']) ? 'good' : 'bad';
        }
      }
    }
    // The collection/s is a little more tricky because there can be more than one
    // value represented by CSV -- and the object's relationships returns an array.
    $batch_sites_as_array = explode(",", $batch_record['mapto_site_id_values']);
    $batch_collections_as_array = explode(",", $batch_record['mapto_collections']);
    $object_collections = $islandora_object->relationships->get('info:fedora/fedora-system:def/relations-external#', 'isMemberOfCollection');
    $object_sites = $islandora_object->relationships->get('http://digital.library.pitt.edu/ontology/relations#', 'isMemberOfSite');
    $batch_collections = $object_rels_collections = $batch_sites = $object_rels_sites = array();
    foreach ($object_collections as $object_collection_relationship) {
      @list($collection_ns, $collection_pid) = explode(":", $object_collection_relationship['object']['value']);
      $collection_pid = trim($collection_pid);
      $object_rels_collections[] = $collection_pid;
    }
    foreach ($batch_collections_as_array as $collection_pid) {
      $collection_class = (array_search($collection_pid, $object_rels_collections) === FALSE) ? 'bad' : 'good';
      $batch_collections[] = '<span class="' . $collection_class . '">' . $collection_pid . '</span>';
    }
    foreach ($object_sites as $object_site_relationship) {
      @list($site_ns, $site_pid) = explode(":", $object_site_relationship['object']['value']);
      $site_pid = trim($site_pid);
      $object_rels_sites[] = $site_pid;
    }
    foreach ($batch_sites_as_array as $site_pid) {
      $site_class = (array_search($site_pid, $object_rels_sites) === FALSE) ? 'bad' : 'good';
      $batch_sites[] = '<span class="' . $site_class . '">' . $site_pid . '</span>';
    }
    return '<div><span class="' . $model_class . '">type = "' .
              $batch_record['default_type_of_resource'] . '"</span>, ' .
              "site/s = " . implode(', ', $batch_sites) . ", collection/s = " . implode(', ', $batch_collections) . "</div>";
  }
  else {
    return '<i>Batch record missing</i><br>';
  }
}

function upitt_workflow_solr_reindex_for_barcodes($barcodes) {
  module_load_include('inc', 'islandora_gsearcher', 'includes/utilities');
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $markup = '<div class="text-report"><h3>Solr reindex for objects</h3>';
  foreach ($check_results['found_pids'] as $key => $pid) {
    $markup .= $pid . " queued to be added to Solr<br>";
    islandora_gsearcher_inform_gsearch($pid, TRUE);
  }
  $markup .= "</div>";
  return $markup;
}

function upitt_workflow_links_to_objects_for_barcodes($barcodes) {
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $markup = '<div class="text-report"><h3>Links to ingested objects</h3>';
  $markup .= '<h2>Found in Islandora</h2><pre class="good">' . implode("\n", $check_results['found_pid_manage_links']) . '</pre>' .
            '<h2>NOT found in Islandora</h2><pre class="bad">' . implode("\n", $check_results['not_found_pids']) . '</pre>';
  $markup .= '</div>';
  return $markup;
}

/**
 * Helper function that will just get the barcodes from the item record
 * that matches this batch - related through the batch_item table.
 *
 * Returns an empty string if no match was found;
 *
 * @param string $batch
 * @param type $link
 * @return string
 */
function upitt_workflow_legacy_barcodes_from_batch($batch, $link) {
  $sql = "SELECT i.do_id as `barcode` " .
        "FROM batch_item bi " .
        "JOIN batch b ON (bi.batch_id = b.batch_id) " .
        "JOIN item i ON (bi.item_id = i.id) " .
        "WHERE b.batch_external_id = '" . mysqli_real_escape_string($link, $batch) . "'";
  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $path = '';
  $batch_barcodes = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['barcode'])) {
      $batch_barcodes[] = $row['barcode'];
    }
  }
  return $batch_barcodes;
}

/**
 * Helper function that will just get the batch file path from the item_file record
 * that matches this barcode.  Returns an empty string if no match was found;
 *
 * @param string $barcode
 * @param type $link
 * @return string
 */
function upitt_workflow_batch_file_path($barcode, $link) {
  $sql = "SELECT itf.path FROM item i JOIN item_file itf ON (itf.item_id = i.id) WHERE i.do_id = '" . mysqli_real_escape_string($link, $barcode) . "' LIMIT 1";
  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $path = '';
  $row = mysqli_fetch_assoc($result);
  if (isset($row['path'])) {
    $pathinfo = pathinfo($row['path']);
    $path = $pathinfo['dirname'];
  }
  return $path;
}

/**
 * Helper function that will just get the item_type name from the core_item record
 * that matches this barcode.  Returns an empty string if no match was found;
 *
 * @param string $barcode
 * @param type $link
 * @return string
 */
function upitt_workflow_item_type_of_barcode($barcode, $link) {
  $sql = "select i.do_id, c.name from core_item i join core_item_type c ON (i.type_id = c.id) where i.do_id = '" . mysqli_real_escape_string($link, $barcode) . "' LIMIT 1";
  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $item_type = '';
  $row = mysqli_fetch_assoc($result);
  if (isset($row['path'])) {
    $pathinfo = pathinfo($row['path']);
    $item_type = $pathinfo['name'];
  }
  return $item_type;
}

/**
 * This will Add to Hopper Ingest queue for barcodes.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_add_to_hopper_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/hopper');
  $barcodes_arr = explode("\n", $barcodes);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Add to Hopper Ingest queue for barcodes</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    if (trim($barcode)) {
      $pid = $ingest_namespace . trim($barcode);
      $added = upitt_workflow_add_barcode_to_hopper($pid, $link);
      if ($added) {
        $markup .= '<span class="good">' . $barcode . '</span><br>';
      }
      else {
        $markup .= '<span class="bad">' . $barcode . ' existed already and could not be added</span><br>';
      }
    }
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This will Remove barcodes from Hopper Ingest queue.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_hopper_remove_from_hopper_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/hopper');
  $barcodes_arr = explode("\n", $barcodes);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Add to Hopper Ingest queue for barcodes</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    if (trim($barcode)) {
      $pid = $ingest_namespace . trim($barcode);
      $removed = upitt_workflow_remove_barcode_from_hopper($pid, $link);
      if ($removed) {
        $markup .= '<span class="good">' . $barcode . ' removed.</span><br>';
      }
      else {
        $markup .= '<span class="bad">' . $barcode . ' did not exist.</span><br>';
      }
    }
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This will Increase Priority for barcodes.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_hopper_increase_priority_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/hopper');
  $barcodes_arr = explode("\n", $barcodes);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Increase Priority for barcodes</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    if (trim($barcode)) {
      $pid = $ingest_namespace . trim($barcode);
      $item_markup = upitt_workflow_hopper_increase_priority($pid, $link);
      if ($item_markup) {
        $markup .= $item_markup;
      }
      else {
        $markup .= '<span class="bad">' . $barcode . ' did not exist</span><br>';
      }
    }
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This will Decrease Priority for barcodes.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_hopper_decrease_priority_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/hopper');
  $barcodes_arr = explode("\n", $barcodes);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Decrease Priority for barcodes</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    if (trim($barcode)) {
      $pid = $ingest_namespace . trim($barcode);
      $item_markup = upitt_workflow_hopper_decrease_priority($pid, $link);
      if ($item_markup) {
        $markup .= $item_markup;
      }
      else {
        $markup .= '<span class="bad">' . $barcode . ' did not exist</span><br>';
      }
    }
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This will List all Pending Hopper items sorted by their weight.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_hopper_list_pending_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/hopper');
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Pending Hopper items</h3>';
  $markup .= "<b>PID</b>\t<b>Files size</b>\t<b>weight</b><br>";

  if ($barcodes) {
    drupal_set_message('Barcode values not needed for this function', 'info');
  }

  $rows = upitt_workflow_get_pending_hopper_queue_items($link);
  foreach ($rows as $hopper_row) {
    $markup .= $hopper_row['PID'] . "\t" . number_format($hopper_row['files_size']) . "\t" . $hopper_row['weight'] . '<br>';
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This mode is tricky -- it will take either a batch or individual barcodes
 * AND will require at least one of the following values set:
 *   site
 *   collection
 *   type
 *
 * If nothing is passed for this mode, it should display the "help" on how to
 * use it.
 *
 * @param type $batches
 * @return string
 */
function upitt_workflow_update_workflow_values_for_barcodes($values) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $values = trim($values);
  $values_arr = explode("\n", $values);
  $barcodes_arr = array();
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $good_lines = $good_nv_pairs = TRUE;
  $legal_nv_names = array('site', 'collection', 'type');
  $short_fieldnames = array('site=','collection=','type=');
  $full_fieldnames = array('mapto_site_id_values=', 'mapto_collections=', 'default_type_of_resource=');
  foreach ($values_arr as $test_value) {
    $test_value = trim($test_value);
    if ($test_value) {
      $good_lines &= ((strstr($test_value, "|") <> '') && (strstr($test_value, "=") <> ''));
      // Split the line value apart into its constituant parts
      @list($barcode_or_batchname, $name_value1, $name_value2, $name_value3) = explode("|", $test_value);
      // Also, test that the name_value parts exist -- and whether or not they also
      // have one of the legal field names
      @list($name_1, $value_1) = explode("=", $name_value1);
      @list($name_2, $value_2) = explode("=", $name_value2);
      @list($name_3, $value_3) = explode("=", $name_value3);
      // at least one name value is needed for each line, so this first one is
      // not optional, but the last two are.
      $good_nv_pairs &= (( $name_1 ? !(array_search($name_1, $legal_nv_names) === FALSE) : FALSE ) &&
                        ( $name_2 ? !(array_search($name_2, $legal_nv_names) === FALSE) : TRUE ) &&
                        ( $name_3 ? !(array_search($name_3, $legal_nv_names) === FALSE): TRUE ));
      $name_value1 = str_replace($short_fieldnames, $full_fieldnames, $name_value1);
      $name_value2 = str_replace($short_fieldnames, $full_fieldnames, $name_value2);
      $name_value3 = str_replace($short_fieldnames, $full_fieldnames, $name_value3);

      // The values provided MAY be true "batches", so try to get the barcodes
      $barcodes = upitt_workflow_legacy_barcodes_from_batch($barcode_or_batchname, $link);
      if (is_array($barcodes)) {
      // update the values to reflect the real field names here

        foreach ($barcodes as $barcode) {
          $barcodes_arr[] = array(
              'barcode' => $barcode,
              'barcode_or_batchname' => $barcode_or_batchname,
              'name_value1' => $name_value1,
              'name_value2' => $name_value2,
              'name_value3' => $name_value3
            );
        }
      }
      else {
        $barcodes_arr[] = array(
              'barcode' => $barcode_or_batchname,
              'barcode_or_batchname' => '',
              'name_value1' => $name_value1,
              'name_value2' => $name_value2,
              'name_value3' => $name_value3
            );
      }
    }
  }

  if (!$values || !$good_lines || !$good_nv_pairs) {
    $markup = '<div class="text-report">' .
              '  <h3>"Update workflow records for batch or barcode" - Usage</h3>' .
              (($values && !$good_lines) ? '<p class="bad"><b>One of the line values did not include either ' .
                 'the equals sign "=" or the pipe character "|".</b><p>' : '') .
              (($values && !$good_nv_pairs) ? '<p class="bad"><b>One of the lines did not contain a valid ' .
                 'field name and could not be processed further.</b><p>' : '') .
              '  <p>This will update the "site", "collection", and "type" value for ' .
              'the workflow records that are used for ingest; these are the <b class="good">only ' .
              'fieldnames</b> supported.</p>' .
              '  <p>Each line should specify either a barcode or a batch name and ' .
              'the name/value pairs that need to be set delimited by the pipe character "|" such as: ' .
              '<b class="good">{BARCODE_or_BATCHNAME}|{fieldname}=value|{fieldname}=value|{fieldname}=value.</b>  ' .
              'To set any of the optional values, the line value must include ' .
              'the name of the field and the value to set for that field.</b>  <p>Examples:</p> ' .
              '<li>31735066984000|site=pitt:site.historic-pittsburgh,pitt:site.uls-digital-collections' .
              '|type=bookCModel</li><li>20160201-ulsnewspaper|collection=' .
              'pitt:collection.3|type=newspaperCModel</li><li>31735066984000|type=bookCModel</li></ul>' .
              '  <p>Allowed values for <b class="good">"site"</b>:<ul><li>any collection PID value such ' .
              'as "pitt:site.uls-digital-collections"</li><li><em>multiple "site" values should be ' .
              'separated by a comma</em></li></ul></p>' .
              '  <p>Allowed values for <b class="good">"collection"</b>:<ul><li>any collection PID value such ' .
              'as "pitt:collection.3" or "pitt:collection.209"</li><li><em>multiple "collection" values should be ' .
              'separated by a comma</em></li></ul></p>' .
              '  <p>Allowed values for <b class="good">"type"</b>:<ul><li>newspaperCModel</li>' .
              '<li>manuscriptCModel</li>' .
              '<li>sp_large_image_cmodel</li>' .
              '<li>bookCModel</li></ul></p>' .
              '</div>';
    mysqli_close($link);
    return $markup;
  }
  $missing_barcodes = array();

  $markup = '<div class="text-report">' .
            '  <h3>"Update workflow records for batch or barcode"</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $last_batch_name = '';
  foreach ($barcodes_arr as $barcode_and_name_values) {
    // 1. load the batch record for the given barcode - and pass to the function that does the copy operation
    $barcode = $barcode_and_name_values['barcode'];
    $batch_record = upitt_workflow_batch_load_by_batch_external_id($barcode);
    if (count($batch_record) < 1 && $barcode) {
      $missing_barcodes[] = $barcode;
    }
    else {
      if ($barcode_and_name_values['barcode_or_batchname'] <> $last_batch_name) {
        $markup .= '<pre><b>' . $barcode_and_name_values['barcode_or_batchname'] . '</b></pre>';
      }
      $markup .= '<pre>' . upitt_workflow_update_set_batchvalues($barcode_and_name_values, $link) . "</pre>";
      $last_batch_name = $barcode_and_name_values['barcode_or_batchname'];
    }
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  mysqli_close($link);
  return $markup;
}

/**
 * This will get all of the barcodes assocaited with legacy batch identifiers.
 *
 * @param type $batches
 * @return string HTML markup
 */
function upitt_workflow_get_wf_barcodes_from_batch_for_barcodes($batches) {
  $batches_arr = explode("\n", $batches);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Bigfoot paths for barcodes</h3>';
  foreach ($batches_arr as $batch) {
    $batch = trim($batch);
    if ($batch) {
      $barcodes = upitt_workflow_legacy_barcodes_from_batch($batch, $link);
      if ($barcodes) {
        $markup .= '<span class="good"><b>batch "' . $batch . '"</b></span><br>';
        $barcodes_str = implode("<br>", $barcodes);
        $barcodes_link = '/workflow/workflowadmin/process_barcodes/?barcodes=' .
          implode(",", $barcodes) . '&mode=checkobjects';
        $markup .= '<b>' . count($barcodes) . ' total barcodes</b>.' .
          ((strlen($barcodes_link) < 2048) ? ' <a href="' . $barcodes_link . '" ' . 
            'target="_blank">Process these barcodes</a>' :
          ' <i>There are too many barcodes to pass as a GET link.</i>');
        $markup .= '<br><span class="good">' . $barcodes_str . '</span><hr><br>';
      }
      else {
        $markup .= '<span class="bad">' . $batch . ' not found in item table</span><br>';
      }
    }
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This will get common Solr values for the barcodes or batch.
 *
 * @param string $values
 *   The posted values to the Process Barcodes form.
 * @return string HTML markup
 */
function upitt_workflow_query_solr_for_barcodes($values) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $missing_barcodes = array();
  $barcode_batch_array = upitt_workflow_barcode_or_batch_barcodes($values);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $batch_record_fields = array(
      'mapto_site_id_values', 
      'mapto_collections',
      'default_type_of_resource',
  );

  $markup = '<div class="text-report"><h3>Bigfoot paths for barcodes</h3>';
  $last_batch_name = '';
  $solr_query_prefix = 'PID:*';
  $key_field = 'PID';
  $value_fields_csv = 'fgs_label_s,RELS_EXT_isMemberOfCollection_uri_ms,RELS_EXT_isMemberOfSite_uri_ms,' . 
                      'RELS_EXT_hasModel_uri_ms,fedora_datastream_latest_OCR_SIZE_ms,fedora_datastream_latest_PDF_SIZE_ms,fgs_createdDate_dt';
  $value_fields_arr = explode(",", $key_field . ',' . $value_fields_csv);
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $rows = array();
  foreach ($barcode_batch_array as $barcode_batch) {
    if (array_key_exists('barcode', $barcode_batch) && $barcode_batch['barcode']) {
      $barcode = trim($barcode_batch['barcode']);
      $pid = $ingest_namespace . $barcode;

      $solr_record = upitt_workflow_get_solr_values($solr_query_prefix . str_replace(":", "%5C%3A", $barcode), $key_field, $value_fields_csv, '', 1);
      $a_row = array();
      foreach ($value_fields_arr as $f) {
        $a_row[$f] = ((array_key_exists($pid, $solr_record)) ? ((array_key_exists($f, $solr_record[$pid])) ?
          ((is_array($solr_record[$pid][$f])) ? implode("\n", $solr_record[$pid][$f]) : $solr_record[$pid][$f]) : 
          '') : '');
      }
      // Before adding the a_row to the $rows array... add the values from the
      // batch record to compare
      @list($namespace, $barcode) = explode(":", $pid);
      $batch_record = upitt_workflow_batch_load_by_batch_external_id($barcode);
      foreach ($batch_record_fields as $batch_field_name) {
        $a_row[$batch_field_name] = (array_key_exists($batch_field_name, $batch_record)) ? $batch_record[$batch_field_name] : '';
      }
      $rows[$pid] = $a_row;
    }
  }

  $markup .= theme('table', array(
     'rows'=>$rows,
     'header'=> array_merge($value_fields_arr, $batch_record_fields),
     'attributes' =>array('class'=>array('report_table'))
  ));

  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This will look in the item / item_file table to get the path for that barcode's workflow record.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_get_wf_paths_for_barcodes($barcodes) {
  $barcodes_arr = explode("\n", $barcodes);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Bigfoot paths for barcodes</h3>';
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $path = upitt_workflow_batch_file_path($barcode, $link);
    if ($path) {
      $markup .= '<span class="good">' . $barcode . "\t" . $path . '</span><br>';
    }
    else {
      $markup .= '<span class="bad">' . $barcode . ' not found in item table</span><br>';
    }
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * This will look in the item / core_item_type table to get the item_type
 * of the barcodes' workflow records.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_get_wf_model_for_barcodes($barcodes) {
  $barcodes_arr = explode("\n", $barcodes);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Bigfoot paths for barcodes</h3>';
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $item_type = upitt_workflow_item_type_of_barcode($barcode, $link);
    if ($item_type) {
      $markup .= '<span class="good">' . $barcode . "\t" . $item_type . '</span><br>';
    }
    else {
      $markup .= '<span class="bad">' . $barcode . ' not found in item table</span><br>';
    }
  }
  $markup .= '</div>';
  mysqli_close($link);

  return $markup;
}

/**
 * Simple function to convert the barcodes to pid values.
 *
 * @param type $vals
 * @return HTML markup
 */
function upitt_workflow_barcode_to_pid_for_barcodes($vals) {
  $barcodes_arr = explode("\n", $vals);
  $markup = '<div class="text-report"><h3>PIDs to barcodes</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $pid = $ingest_namespace . trim($barcode);
    $markup .= $pid . '<br>';
  }
  $markup .= '</div>';

  return $markup;
}

/**
 * Simple function to convert the pid values to barcodes.
 *
 * @param type $vals
 * @return HTML markup
 */
function upitt_workflow_pid_to_barcode_for_barcodes($vals, $raw_output = FALSE) {
  $pids_arr = explode("\n", $vals);
  $markup = ($raw_output ? '' : '<div class="text-report"><h3>PIDs to barcodes</h3>');
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $barcodes = array();
  foreach ($pids_arr as $pid) {
    $pid = trim($pid);
    if ($pid) {
      $barcodes[] = str_replace($ingest_namespace, "", $pid);
    }
  }
  $markup .= implode(($raw_output ? "\n" : '<br>'), $barcodes);
  if (!$raw_output) {
    $markup .= '</div>';
  }

  return $markup;
}

/**
 * 
 *
 * @param type $barcodes
 * @return string
 */
function upitt_workflow_purge_objects_for_barcodes($barcodes) {
  module_load_include('inc', 'islandora', 'includes/manage_deleted_objects');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $pages_delete_statuses = array();
  $deleted = FALSE;
  foreach ($check_results['found_pids'] as $key => $pid) {
    if ($pid) {
      $islandora_object = islandora_object_load($pid);
      if (is_object($islandora_object)) {
        $is_paged = upitt_workflow_is_paged_object($islandora_object);
        if ($is_paged) {
          $child_pages = islandora_paged_content_get_pages_ri($islandora_object);
          foreach ($child_pages as $child_page) {
            if (isset($child_page['pid'])) {
              $child = islandora_object_load($child_page['pid']);
              if (is_object($child)) {
                $deleted = islandora_delete_object($child);
              }
              else {
                $deleted = FALSE;
              }
              $pages_delete_statuses[] = $child_page['pid'] . ' ' . ($deleted ? 'deleted' : 'not deleted') ;
            }
          }
        }
        $deleted = islandora_delete_object($islandora_object);
        $check_results['found_pid_links'][$key] = $pid . ' ' . ($deleted ? 'deleted' : 'not deleted') ;
      }
    }
  }
  foreach ($check_results['not_found_pids'] as $key => $pid) {
    if ($pid) {
      // Try a SOLR query to get child PID values that are not attached to the book (orphans)
      $solr_query = "PID:" . str_replace(":", "\:", $pid) . "*";
      $child_pids = upitt_workflow_get_solr_options($solr_query, 'PID', 'PID');
      if (strstr($pid, '-') == '') {
        for ($i = 1; $i < 50; $i++) {
          $seq = $pid . (($i < 10) ? '-000' . $i : '-00' . $i);
          if (!array_key_exists($seq, $child_pids)) {
            $child_pids[$seq] = $seq;
          }
        }
      }
      foreach ($child_pids as $k => $child_pid) {
        if (isset($child_pid)) {
          $child = islandora_object_load($child_pid);
          if (is_object($child)) {
            $deleted = islandora_delete_object($child);
          }
          else {
            $deleted = FALSE;
          }
          $pages_delete_statuses[] = $child_pid . ' ' . ($deleted ? 'deleted' : 'not deleted') ;
        }
      }
    }
  }
  $markup = '<div class="text-report"><h3>Deleted objects</h3><div class="found">' . implode("<br>", $check_results['found_pid_links']) . '</div><hr>' .
            '<h3>Deleted child objects</h3><div class="found">' . implode("<br>", $pages_delete_statuses) . '</div>' . 
            '<h3>NOT found in Islandora</h3><div class="not-found">' . implode("<br>", $check_results['not_found_pids']) . '</div></div>';
  return $markup;
}

function upitt_workflow_do_publish_for_barcodes($values) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'upitt_workflow', 'includes/batch.form');

  $missing_barcodes = array();
  $values_arr = explode("\n", $values);
  $barcodes_arr = array();
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  foreach ($values_arr as $value) {
    // The values provided MAY be true "batches", so try to get the barcodes  
    $barcodes = upitt_workflow_legacy_barcodes_from_batch($value, $link);
    if (is_array($barcodes)) {
      foreach ($barcodes as $barcode) {
        $barcodes_arr[] = $barcode;
      }
    }
    else {
      $barcodes_arr[] = $value;
    }
  }

  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    // 1. load the batch record for the given barcode - and pass to the function that does the copy operation
    $batch_record = upitt_workflow_batch_load_by_batch_external_id($barcode);
    if (count($batch_record) < 1 && $barcode) {
      $missing_barcodes[] = $barcode;
    }
    $markup .= '<div>' . upitt_workflow_do_publish_using_batchvalues($batch_record, $ingest_namespace, $barcode) . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  mysqli_close($link);
  return $markup;
}

function upitt_workflow_do_publish_using_batchvalues($batch, $ingest_namespace, $barcode) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  if (is_array($batch) && array_key_exists('batch_external_id', $batch)) {
    $object_id = $batch['batch_external_id'];
    $sites = $batch['mapto_site_id_values'];
    $collections = $batch['mapto_collections'];
  }
  else {
    $object_id = $barcode;
    $sites = '';
    $collections = '';
  }

  $sites_arr = explode(",", $sites);
  $collections_arr = explode(",", $collections);
  upitt_workflow_relate_object_to_collections_sites($object_id, $sites_arr, $collections_arr);
}

function upitt_workflow_do_copy_for_barcodes($barcodes, $measure_size_only = FALSE, $verbose = TRUE) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'upitt_workflow', 'includes/batch.form');

  $avail_ingest_temp_space = $grand_total_size = 0;
  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = ($verbose) ? '<div class="text-report">' : '';
  $avail_ingest_temp_space = upitt_workflow_get_ingest_temp_space();
  $ingest_namespace = upitt_workflow_get_ingest_namespace();

  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    if ($barcode) {
      // 1. load the batch record for the given barcode - and pass to the function that does the copy operation
      $batch = upitt_workflow_batch_load_by_batch_external_id($barcode);
      if (count($batch) < 1 && $barcode) {
        $missing_barcodes[] = $barcode;
      } else {
        $do_copy = TRUE;
        $do_download = (strstr($batch['batch_host'], 'ftp:'));
        $content_model = isset($batch['default_type_of_resource']) ? $batch['default_type_of_resource'] : 'sp_large_image_cmodel';
        $batch_external_id = $batch['batch_external_id'];

        // Now it is safe to make the full path to the specific incoming subfolder that has been selected.
        $incoming_folder = (isset($batch['batch_host']) && strstr($batch['batch_host'], 'ftp:')) ?
              $batch['batch_host'] :
              (isset($batch['batch_host']) ? $batch['batch_host'] :
              rtrim(variable_get('upitt_workflow_ingest_incoming', '/ingest/incoming'), '/') . '/' . $incoming_folder);
        if ($incoming_folder == '' || $batch_external_id == '') {
          drupal_set_message('The batch name and a incoming folder must be set in order to save this batch', 'error');
          return;
        }
        $batch_path = rtrim(variable_get('upitt_workflow_ingest_prepared_path', '/ingest/islandora_ingest'), '/') . '/batch_' . $batch_external_id;
        if ($content_model == 'compoundCModel') {
          $compound_objects_folder = rtrim(variable_get('upitt_workflow_ingest_prepared_path', '/ingest/islandora_ingest'), '/') . '/compound_objects';
          if (!file_exists($compound_objects_folder)) {
            mkdir($compound_objects_folder);
          }
          $batch_path = $compound_objects_folder . '/batch_' . $batch_external_id;
        }

        $copyfiles_result = upitt_workflow_copyfiles_incoming_folder_to_batch($incoming_folder, $batch_path, $content_model, $do_copy, $do_download, $batch_external_id, $measure_size_only);

        if ($content_model == 'compoundCModel') {
          // For Compound objects, it will need to run create_structure_files.php to make
          // a structure.xml for each parent compound object.
          $compound_batch_module_full_path = getcwd() . '/' . drupal_get_path('module','islandora_compound_batch') . '/extras/scripts';
          // make the command to call
          $compound_objects_containing_folder = str_replace('/batch_' . $batch_external_id, '', $batch_path);
          $create_structure_command = 'cd ' . $compound_batch_module_full_path . ';php ' . $compound_batch_module_full_path . '/create_structure_files.php ' . $compound_objects_containing_folder;
          $output = array();
          exec($create_structure_command, $output, $return_var);
          if (count($output) > 0) {
            foreach ($output as $subfolder) {
              $copyfiles_result['markup'] .= 'structure.xml created for compound object folder ' . $subfolder . "<br>";
            }
          }
        }

        if ($verbose) {
          $markup .= isset($copyfiles_result['markup']) ? '<div>' . $copyfiles_result['markup'] . '</div>' : '';
        }
        else {
          $pid = $ingest_namespace . $barcode;
          $markup .= (isset($copyfiles_result['total_measured_space'])) ? $pid . '=' . $copyfiles_result['total_measured_space'] . "\n" : 0;
        }
        $grand_total_size += (isset($copyfiles_result['total_measured_space'])) ? $copyfiles_result['total_measured_space'] : 0;
        $avail_ingest_temp_space = $copyfiles_result['available_space'];
      }
    }
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  if ($verbose) {
    $markup .= '</div>';

    // Insert the total file size result at the top in a new <div>
    $markup = '<div class="text-report"><h3>Grand Total of File Size of Ingest Files</h3>' .
            '  <div class="not-found"><b>Total measured space: ' . upitt_workflow_formatBytes($grand_total_size,2) . '</b></div>' .
            '  <div class="not-found"><b>Total AVAILABLE space: ' . upitt_workflow_formatBytes($avail_ingest_temp_space,2) . '</b></div>' .
            '  <div class="' . (($grand_total_size < $avail_ingest_temp_space) ? 'good' : 'bad') . '"><b>This ingest represents ' .
            (($avail_ingest_temp_space > 0) ? round($grand_total_size / $avail_ingest_temp_space * 100, 2) : '<i>an unknown </i>') . '% of the available space</b></div>' .
            '</div>'.
            $markup;
  }

  return $markup;
}


function upitt_workflow_gen_OCR_for_barcodes($barcodes) {
  $markup = upitt_workflow_purge_OCR_PDF_for_barcodes($barcodes);
  global $user;
  $barcodes_arr = explode("\n", $barcodes);

  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  $markup .= '<div class="text-report">';
  $markup .= '<h2>Generate OCR derivatives for These Barcodes</h2><pre style="color:blue">' . implode("\n", $barcodes_arr) . '</pre><hr>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();

  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $this_domain = 'http://' . $_SERVER['HTTP_HOST'];
    $pid = $ingest_namespace . $barcode;

    $islandora_object = islandora_object_load($pid);
    if (!is_object($islandora_object)) {
      $markup .= '<div class="bad">Object not found ' . $pid . '</div>';
    }
    if (!upitt_workflow_is_paged_object($islandora_object)) {
      $markup .= '<div class="bad">Object found ' . $pid . ', but it is not a paged content object.</div>';
    }
    $drush_command = 'drush -v -u ' . $user->uid . ' --uri=' . $this_domain . ' upitt_workflow_generate_ocr_datastreams ' .
                     '--paged_object_pid=' . $pid;
    $markup .= '<h4>Called upitt_workflow_generate_ocr_datastreams</h4><code>' . $drush_command . '</code>';

    $output = $return_var = array();
    exec($drush_command, $output, $return_var);

    if (($return_var == -1 || $return_var == 255 || $return_var == 1) && !$alerted_on_err) {
      drupal_set_message('There was an error calling the drush command : ' . $drush_command);
    }
    $markup .= "<hr>drush command return = " . $return_var . "<br>drush command output = " . print_r($output, true) . "</pre>";
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_ingest_prepare_for_barcodes($barcodes) {
  global $user;
  $barcodes_arr = explode("\n", $barcodes);

  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
  $markup = '<div class="text-report">';
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
        $drush_generic_markup = upitt_workflow_drush_generic($batch);
        $set = upitt_workflow_get_set($batch['batch_external_id']);
        if ($set > 0) {
          upitt_workflow_set_status_for_batch($batch['batch_external_id'], UPITT_WORKFLOW_ACTION_ISLANDORA_INGEST_PREPARED, NULL, TRUE);
        }

        $this_domain = 'http://' . $_SERVER['HTTP_HOST'];
        $ingest_set_drush_command = 'drush -v -u ' . $user->uid . ' --uri=' . $this_domain . ' islandora_batch_ingest --ingest_set=' . $set;

        if ($set <> 0) {
          // no need to display the output of $drush_generic_markup since the
          // $set value is valid.
          // NOW, we can call the ingest command.
          $output = $return_var = array();
          exec($ingest_set_drush_command, $output, $return_var);

          if (($return_var == -1 || $return_var == 255) && !$alerted_on_err) {
            drupal_set_message('There was an error calling the drush command : ' . $ingest_set_drush_command);
            $alerted_on_err = TRUE;
          }
          $color = (($return_var == -1 || $return_var == 255)) ? 'red' : 'green';
          $markup .= '(called islandora_batch_ingest)<code style="color:' . $color . '">' . $ingest_set_drush_command . '</code>';
          $markup .= "<code style='color:#598'>" . $drush_generic_markup . "</code>";
        }
        else {
          // the $set was 0... there was a problem with the earlier prepare
          // command from the upitt_workflow_drush_generic
          $markup .= '<div class="bad">' . $drush_generic_markup . '</div>';
        }
      }
      else {
        $markup .= '<span class="bad">batch not found for $barcode</span><br>';
      }
    }
    else {
      $markup .= '<span class="bad">batch_id could not be found for ' . $barcode . '</span><br>';
    }
  }
  $markup .= '</div>';
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

function upitt_workflow_aggregate_for_barcodes($barcodes) {
  module_load_include('inc', 'islandora', 'includes/manage_deleted_objects');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $missing_pids = $pages_delete_statuses = array();
  $markup = '<div class="text-report">';
  foreach ($check_results['found_pids'] as $key => $pid) {
    $ingested_object = islandora_object_load($pid);
    if (is_object($ingested_object)) {
      $markup .= '<div class="good"><b>' . $pid . '</b></div>';
      // See whether or not this is a paged-content object in order to call the
      // "aggregate PDF" and "aggregate OCR/HOCR" batch commands on it now.
      if (upitt_workflow_is_paged_object($ingested_object)) {
        upitt_workflow_aggregate_pdf_derivative($ingested_object);
        $markup .= 'PDF aggregate created<br>';
        upitt_workflow_aggregate_ocr_derivative($ingested_object);
        $markup .= 'OCR aggregate created<br>';

        $is_paged = upitt_workflow_is_paged_object($ingested_object);
        $pages = ($is_paged) ? islandora_paged_content_get_pages($ingested_object) : array();
        if (count($pages)) {
          module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');
          $page = array_shift($pages);
          $page_object = islandora_object_load($page['pid']);
          upitt_workflow_make_tn_large($page_object, $ingested_object);
          $markup .= 'TN_LARGE created<br>';
        }
      }
    }
    else {
      $markup .= '<div class="bad"><b>' . $pid . '</b> missing</div>';
      $missing_pids[] = $pid;
    }
  }
  if (count($missing_pids) > 0) {
    drupal_set_message('Some PID values could not be found ' . implode(", ", $missing_pids), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

/**
 * Helper function to each line as of input as either a batch or a barcode.
 *
 * @param string $values
 *   The posted values to the Process Barcodes form.
 * @return type
 *   array of array values that contain 'batch' and barcode of each.  When a
 * barcode is passed, the batch of that barcode is not returned.
 */
function upitt_workflow_barcode_or_batch_barcodes($values) {
  $values_arr = explode("\n", $values);
  $barcode_batch_array = array();
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  foreach ($values_arr as $value) {
    $value = trim($value);
    if ($value) {
      // The values provided MAY be true "batches", so try to get the barcodes
      $barcodes = upitt_workflow_legacy_barcodes_from_batch($value, $link);
      if (is_array($barcodes) && count($barcodes) > 0) {
        foreach ($barcodes as $barcode) {
          $barcode_batch_array[] = array(
            'batch' => $value,
            'barcode' => $barcode,
          );
        }
      }
      else {
        $barcode_batch_array[] = array(
            'batch' => '',
            'barcode' => trim($value),
          );
      }
    }
  }
  mysqli_close($link);
  return $barcode_batch_array;
}
