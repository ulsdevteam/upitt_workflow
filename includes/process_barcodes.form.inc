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
  if (!isset($form_state['values']['barcodes']) && isset($_GET['barcodes']) && isset($_GET['mode'])) {
    $form_state['values']['barcodes'] = filter_xss($_GET['barcodes']);
    $form_state['values']['mode'] = filter_xss($_GET['mode']);
  }
  $process_options = array(
    '0' => 'Standard Ingest Steps',
      'delete_update_copy_ingest' => ' - One step (delete, update, copy, ingest)',
      'update' => ' - Update batch records (no overwrite if islandora object exists)',
      'updateforce' => ' - Update batch records (force overwrite)',
      'checksize' => ' - Check file size of ingest files',
      'do_copy' => ' - Copy files from incoming to ingest location',
      'ingest_prepare' => ' - Islandora Ingest Preprocess',
      'gen_OCR' => ' - Generate OCR',
      'do_publish' => ' - Publish objects to site',
    '1' => 'Special Functionality',
      'make_tn' => ' - Make TN derivative',
      'make_tn_large' => ' - Make TN_LARGE derivative',
      'make_jp2' => ' - Make JP2 derivative',
      'make_page_pdfs' => ' - (Make page PDF derivatives)',
      'aggregate' => ' - Aggregate OCR and PDF, make TN_LARGE',
      'set_status' => ' - Set Status',
    '2' => 'Info / Cleanup',
      'checkobjects' => ' - Check for Islandora Objects',
      'links_to_objects' => ' - Admin links to objects',
      'deep_info_on_objects' => ' - Deep info on objects',
      'get_wf_paths' => ' - Get Bigfoot paths',
      'purge_OCR_PDF' => ' - Purge page OCR and PDF derivative',
      'delete_batch' => ' - DELETE islandora_batch_* records',
      'purge_objects' => ' - DELETE objects',
      'pid_to_barcode' => ' - Convert PIDs to barcodes',
      'barcode_to_pid' => ' - Convert barcodes to PIDs',
  );
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
  switch ($btn_id) {
    case 'edit-submit':
      if ($mode == 'update' || $mode == 'updateforce') {
        $markup = upitt_workflow_make_batch_records_for_barcodes($form_state['values']['barcodes'], $mode);
      } elseif ($mode == 'checksize') {
        $markup = upitt_workflow_do_copy_for_barcodes($form_state['values']['barcodes'], TRUE);
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
function upitt_workflow_check_islandora_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
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
        $pages = islandora_paged_content_get_pages($islandora_object);
        $page_count = count($pages);
        $found_pid_manage_links[$barcode] = '"' . $pid . '": ' .
                ($page_count ? '(' . number_format($page_count) . ' pages) ' : '') .
                l('DELETE', 'islandora/object/' . $pid . '/delete', array('attributes' => array('title' => 'DELETE object', 'class' => 'bad'))) . ' | ' .
                l('Regen', 'islandora/object/' . $pid . '/regenerate', array('attributes' => array('title' => 'Regenerate all derivatives', 'class' => 'bad'))) . ' | ' .
                l('Manage', 'islandora/object/' . $pid . '/manage', array('attributes' => array('title' => 'Manage object'))) . ' | ' .
                l('Datastreams', 'islandora/object/' . $pid . '/manage/datastreams', array('attributes' => array('title' => 'Manage | Datastreams'))) . ' | ' .
                l('Workflow' , 'workflow/object/' . $barcode, array('attributes' => array('title' => 'View workflow record for this object'))) . ' | ' .
                l('Publish' , 'workflow/object/' . $barcode . '/do_publish', array('attributes' => array('title' => 'Publish object to sites and relate to collections'))) . ' | ' .
                l('"' . $islandora_object->label . '"', 'islandora/object/' . $pid, array('attributes' => array('title' => 'Navigate to "' . htmlspecialchars($islandora_object->label) . '"')));
        $found_pid_links[$barcode] = '"' . $pid . '": ' .
                l('"' . $islandora_object->label . '"', 'islandora/object/' . $pid);
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
    mysqli_query($link, $sql);

    $batch_id = upitt_workflow_get_batch_id($barcode , FALSE);
    $item_id = upitt_workflow_insert_item_for_batch_external_id($barcode, $link);
    if (!$batch_id) {
      $batch_id = upitt_workflow_get_batch_id($barcode);
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
  $checksize_markup = upitt_workflow_do_copy_for_barcodes($barcodes, TRUE);
  return  (strstr($checksize_markup, 'Not enough space') == '') ?
    $checksize_markup .
    upitt_workflow_delete_batch_for_barcodes($barcodes) .
    upitt_workflow_purge_objects_for_barcodes($barcodes) .
    upitt_workflow_make_batch_records_for_barcodes($barcodes, 'update') .
    upitt_workflow_do_copy_for_barcodes($barcodes) .
    upitt_workflow_ingest_prepare_for_barcodes($barcodes) :
    $checksize_markup;
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
    $sql_sets = "select q.sid, q.id from islandora_batch_queue q join islandora_batch_set s ON (s.id = q.sid) where q.id = '" . $pid . "'";
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
      $pages = islandora_paged_content_get_pages_ri($islandora_object);
      upitt_workflow_purge_childpages_datastreams($islandora_object, $pages, 'OCR');
      upitt_workflow_purge_childpages_datastreams($islandora_object, $pages, 'PDF');
      $markup .= '<div class="good">Successfully purged OCR or PDF datastream for ' . $pid . '.</div>';
    }
    else {
      $missing_barcodes[] = $barcode;
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
      $pages = islandora_paged_content_get_pages($islandora_object);
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
      $missing_barcodes[] = $barcode;
    }
    $markup .= '<div class="good"><b>PDF derivatives created for the pages of ' . $pid . "</b></div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_make_jp2_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($barcodes_arr as $barcode) {
    $barcode = trim($barcode);
    $pid = $ingest_namespace . $barcode;
    $islandora_object = islandora_object_load($pid);
    if (is_object($islandora_object)) {
      if (!upitt_workflow_make_jp2($islandora_object, $islandora_object)) {
        $missing_barcodes[] = $barcode;
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
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_make_tn_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/islandora_ingest');

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
      $missing_barcodes[] = $barcode;
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
    }
    else {
      $missing_barcodes[] = $barcode;
    }
    $markup .= '<div>TN_LARGE created for ' . $pid . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
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

function upitt_workflow_datastream_results($islandora_object, $ingest_namespace, $is_child = FALSE) {
  $barcode = str_replace($ingest_namespace, "", $islandora_object->id);
  $markup = '<div class="good">' . $islandora_object->id . ': ' .
    l('DELETE', 'islandora/object/' . $islandora_object->id . '/delete', array('attributes' => array('title' => 'DELETE object', 'class' => 'bad'))) . ' | ' .
    l('Regen', 'islandora/object/' . $islandora_object->id . '/regenerate', array('attributes' => array('title' => 'Regenerate all derivatives', 'class' => 'bad'))) . ' | ' .
    l('Manage', 'islandora/object/' . $islandora_object->id . '/manage', array('attributes' => array('title' => 'Manage object'))) . ' | ' .
    l('Datastreams', 'islandora/object/' . $islandora_object->id . '/manage/datastreams', array('attributes' => array('title' => 'Manage | Datastreams'))) . ' | ' .
    l('Workflow' , 'workflow/object/' . $barcode, array('attributes' => array('title' => 'View workflow record for this object'))) . ' | ' .
    ($is_child ? '' : l('Publish' , 'workflow/object/' . $barcode . '/do_publish', array('attributes' => array('title' => 'Publish object to sites and relate to collections'))) . ' | ') .
    ($is_child ? '' : '<a href="/workflow/workflowadmin/process_barcodes/?barcodes=' . $barcode . '&mode=aggregate">Aggregate OCR PDF</a> | ') . 
    l('"' . $islandora_object->label . '"', 'islandora/object/' . $islandora_object->id) .
    '</div><div class="datastreams">';
  $ds_arr = array();
  $pdf_size = FALSE;
  $has_OCR = $has_PDF = $has_TN_LARGE = FALSE;
  $size_OCR = 0;
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
//  return array('markup' => $markup . 
//               '<div class="wide_200 fl pl_20">' . implode(", ", $ds_ids) . '</div>' .
//               '</div><br>', 'OCR_size' => $size_OCR);
  return array('markup' => $markup . implode(", ", $ds_ids) . '</div>', 'OCR_size' => $size_OCR, 'pdf_size' => $pdf_size);
}

function upitt_workflow_child_object_results($object, $ingest_namespace) {
  // if paged object, then use the pages function to get the child page objects
  $markup = '';
  $object_members = upitt_workflow_get_object_members_solr_record($object->id,'PID');
  $total_OCR_size = 0;
  foreach ($object_members as $member_solr_doc) {
    $member_object = islandora_object_load($member_solr_doc['PID']);
    if (is_object($member_object)) {
      $results = upitt_workflow_datastream_results($member_object, $ingest_namespace, TRUE);
      $total_OCR_size += $results['OCR_size'];
      $markup .= $results['markup'] . "<br>";
    }
    else {
      $markup .= '<i class="bad">' . $member_solr_doc['PID'] . ' could not load</i><hr>';
    }
  }
  return array('markup' => ($markup ? '<br><div class="pl_20"><b>object members</b><br>' . $markup . '</div>' : '<i class="bad">no object members</i><br>'),
      'total_OCR_size' => $total_OCR_size);
}

function upitt_workflow_deep_info_on_objects_for_barcodes($barcodes) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  $markup = '<div class="text-report"><h3>Deep info on objects</h3>';
  $check_results = upitt_workflow_check_islandora_for_barcodes($barcodes);
  $ingest_namespace = upitt_workflow_get_ingest_namespace();

  foreach ($check_results['found_pids'] as $barcode => $pid) {
    $islandora_object = islandora_object_load($pid);
    $ocr_aggregate_size = (isset($islandora_object['OCR'])) ? $islandora_object['OCR']->size : 0;
    $requirements = islandora_get_datastreams_requirements_from_content_model($islandora_object);
    $markup .= '<div>' . implode($requirements) . '</div>';
    $datastream_results = upitt_workflow_datastream_results($islandora_object, $ingest_namespace, FALSE);
    $child_results = upitt_workflow_child_object_results($islandora_object, $ingest_namespace);
    if ($datastream_results['pdf_size'] === FALSE) {
      $markup .= '<div class="bad">There is no PDF datastream.</div>';
    }
    elseif ($datastream_results['pdf_size'] < 2193) {
      $markup .= '<div class="bad">PDF file is less than 2,193 bytes -- size = ' . $datastream_results['pdf_size'] . '.</div>';
    } 
    else {
      $markup .= '<div class="good">PDF file size is good - size = ' . $datastream_results['pdf_size'] . '.</div>';
    }

    $markup .= $datastream_results['markup'];
    // check the total OCR size of child objects - if this is 0, skip this step because aggregate OCR already occurred.
    $ocr_size_ratio = (($ocr_aggregate_size) ? $child_results['total_OCR_size'] / $ocr_aggregate_size : FALSE);
    if ($child_results['total_OCR_size'] && $ocr_aggregate_size <> $child_results['total_OCR_size']) {
      $markup .= '<div class="bad">OCR aggregate size not equal to total size of all member OCR</div>';
      $markup .= '<div class="bad pl">OCR aggregate = ' . $ocr_aggregate_size . ', total size of all member OCR = ' . $child_results['total_OCR_size'] . '</div>';
    }
    $markup .= $child_results['markup'] . "<hr>";
  }
  $markup .= '<h2>Found in Islandora</h2><pre class="good">' . implode("\n", $check_results['found_pid_manage_links']) . '</pre>' .
            '<h2>NOT found in Islandora</h2><pre class="bad">' . implode("\n", $check_results['not_found_pids']) . '</pre>';
  $markup .= '</div>';
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
 * This will look in the item / item_file table to get the path for that barcode's workflow record.
 *
 * @param type $barcodes
 * @return string HTML markup
 */
function upitt_workflow_get_wf_paths_for_barcodes($barcodes) {
  $barcodes_arr = explode("\n", $barcodes);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $markup = '<div class="text-report"><h3>Bigfoot paths for barcodes</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
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
function upitt_workflow_pid_to_barcode_for_barcodes($vals) {
  $pids_arr = explode("\n", $vals);
  $markup = '<div class="text-report"><h3>PIDs to barcodes</h3>';
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  foreach ($pids_arr as $pid) {
    $barcode = str_replace($ingest_namespace, "", trim($pid));
    $markup .= $barcode . '<br>';
  }
  $markup .= '</div>';

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
              $deleted = islandora_delete_object($child);
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
      foreach ($child_pids as $k => $child_pid) {
        if (isset($child_pid)) {
          $child = islandora_object_load($child_pid);
          $deleted = islandora_delete_object($child);
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

function upitt_workflow_do_publish_for_barcodes($barcodes) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'upitt_workflow', 'includes/batch.form');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
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
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
  return $markup;
}

function upitt_workflow_do_publish_using_batchvalues($batch, $ingest_namespace) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $object_id = $batch['batch_external_id'];
  $sites = $batch['mapto_site_id_values'];
  $collections = $batch['mapto_collections'];

  $sites_arr = explode(",", $sites);
  $collections_arr = explode(",", $collections);
  upitt_workflow_relate_object_to_collections_sites($object_id, $sites_arr, $collections_arr);
}

function upitt_workflow_do_copy_for_barcodes($barcodes, $measure_size_only = FALSE) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'upitt_workflow', 'includes/batch.form');

  $missing_barcodes = array();
  $barcodes_arr = explode("\n", $barcodes);
  $markup = '<div class="text-report">';
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
    $markup .= '<div>' . upitt_workflow_copyfiles_incoming_folder_to_batch($incoming_folder, $batch_path, $content_model, $do_copy, $do_download, $batch_external_id, $measure_size_only) . "</div>";
  }
  if (count($missing_barcodes) > 0) {
    drupal_set_message('Some barcodes could not be found ' . implode(", ", $missing_barcodes), 'warning');
  }
  $markup .= '</div>';
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
    $markup = '<h4>Called upitt_workflow_generate_ocr_datastreams</h4><code>' . $drush_command . '</code>';

    $output = $return_var = array();
    exec($drush_command, $output, $return_var);

    if (($return_var == -1 || $return_var == 255) && !$alerted_on_err) {
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
        $markup .= upitt_workflow_drush_generic($batch);
        $set = upitt_workflow_get_set($batch['batch_external_id']);
        if ($set > 0) {
          upitt_workflow_set_status_for_batch($batch['batch_external_id'], UPITT_WORKFLOW_ACTION_ISLANDORA_INGEST_PREPARED, NULL, TRUE);
        }

        $this_domain = 'http://' . $_SERVER['HTTP_HOST'];
        $ingest_drush_command = 'sudo -u apache drush -v -u ' . $user->uid . ' --uri=' . $this_domain . ' islandora_batch_ingest';
        $ingest_set_drush_command = 'sudo -u apache drush -v -u ' . $user->uid . ' --uri=' . $this_domain . ' islandora_batch_ingest --ingest_set=' . $set;
        $markup .= '<h4>Subsequent call to actually ingest the queued object/s.</h4><code>' . $ingest_drush_command . '<br>' .
                 '<i># or if only one set to be processed, supply the ingest_set option</i><br>' . $ingest_set_drush_command .
                 '</code>';
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

        $pages = islandora_paged_content_get_pages($ingested_object);
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
