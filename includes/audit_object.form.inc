<?php

function upitt_workflow_object_manage_audit($islandora_object) { 
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  if (is_object($islandora_object)) {
    $is_paged = upitt_workflow_is_paged_object($islandora_object);

    $audit_record = upitt_workflow_lookup_audit_record($islandora_object->id);
    $solr_record = upitt_workflow_get_solr_record($islandora_object->id);    
    $solr_record_analysis_table_markup = upitt_workflow_analyze_solr_record($solr_record, $islandora_object->id);

    // check against the "all_ingests.txt" file
    $all_ingests_file = file(drupal_get_path('module', 'upitt_workflow') . '/includes/all_ingests.txt');
    @list($namespace, $id_no_namespace) = explode(":", $islandora_object->id);

    // For a paged object, just look for the pid value ($id_no_namespace) in the all_intests file, but for a 
    // page or manuscript page the parent object should be on the all_ingests list.
    if (!$is_paged) { 
      @list($id_no_namespace, $page_number) = explode("-", $id_no_namespace);
      // look for a matching record in the workflow database
      $workflow_object_file_records = upitt_workflow_get_item_files($id_no_namespace);
      $workflow_page_record_exists = FALSE;
      $record_name = $page_number . '.tif';
      foreach ($workflow_object_file_records as $workflow_object_file) {
        if ($workflow_object_file['use'] == 'MASTER') {          
          $workflow_page_record_exists |= ($workflow_object_file['name'] == $record_name);
        }
      }
    }

    $found = FALSE;
    foreach ($all_ingests_file as $l) {
      if (!$found) {
        $found |= (trim($l) == $id_no_namespace);
      }
    }

    $markup = '<h3>Object Audit</h3> ' .
              '<b>Islandora Object PID:</b> ' . $islandora_object->id . '<br>' . 
              '<b>Model:</b>' . implode(', ', $islandora_object->models) . '<br>';              
    $markup .= '<h4>Object "' . $id_no_namespace . '" ingested upstairs?</h4>' .
              '<p>' . l($id_no_namespace, '/workflow/object/' . $id_no_namespace) . ' is ' . ($found ? '' : 'not') . ' in the "all_ingests.txt" file. ' . 
              (($is_paged) ? '' : 'This page object (' . $islandora_object->id . ') ' . (($workflow_page_record_exists) ? 'has' : 'DOES NOT HAVE') . ' a matching record in the workflow database') . 
              '</p>';
    if (isset($audit_record['markup']) && $audit_record['batch_item_count'] > 0) {
      $markup .= '<h4>Workflow Values for "' . $islandora_object->id . '"</h4>' .
                 $audit_record['markup'] . '<hr>' .
                 '<p>Based on the records for this object in the Workflow system, there should be the following number of objects.</p>' .
                 '<div style="color:#934">' . l(number_format($audit_record['batch_item_count']) . ' total objects in this batch', '/workflow/batch/' . $audit_record['batch_id']) . '</div>' .
                 '<div style="color:#934">' . number_format($audit_record['object_item_count']) . ' items in this object' . '</div>';
      // Look at the number of pages - compare to the batch_item_count for this object.
      if ($is_paged) {
        $child_pages = $is_paged ? islandora_paged_content_get_pages_ri($islandora_object) : array();
        if (count($child_pages) <> $audit_record['object_item_count']) {
          $markup .= '<div class="messages error"><b>Page count mismatch.</b>  The number of items from the workflow (' . 
                     l(number_format($audit_record['object_item_count']) . ' workflow items', '/workflow/batch/' . $audit_record['batch_id']) . 
                     ') do not match the number of pages that exist for this object (' . number_format(count($child_pages)) . ' pages).</div>';
          $markup .= upitt_workflow_audit_analyze_pagecount_mismatch($audit_record, $child_pages, $islandora_object->id);
        }
        else {
          $markup .= '<div class="messages status">Page count matches the expected workflow item count.</div>';
        }
      }
    }
    else {
      $markup .= '<h1>No Workflow records for "' . $islandora_object->id . '"' . '</h4>';
    }
    $markup .= ($solr_record_analysis_table_markup) ? '<hr><h4>Datastream history</h4>' . $solr_record_analysis_table_markup : '';
    return $markup;
  }
}

/**
 * This will look at the values in the Solr record for purposes of displaying a table of the datatream updates.
 */
function upitt_workflow_analyze_solr_record($solrRecord, $pid) {
  // get all version, mimetype, filesize values for each datastream $dsid like fedora_datastream_version_MODS_CREATED_ms
  $rows = $headings = array();
  $datastreams = $solrRecord['fedora_datastreams_ms'];
  $headings = array('ID', 'version ID', 'LABEL', 'CREATED', 'MIMETYPE', 'SIZE', 'Operations');
  
  $messages = array();
  foreach ($datastreams as $dsid) {
    $solrfield_ID = 'fedora_datastream_version_' . $dsid . '_ID_ms';
    $solrfield_LABEL = 'fedora_datastream_version_' . $dsid . '_LABEL_ms';
    $solrfield_CREATED = 'fedora_datastream_version_' . $dsid . '_CREATED_ms';
    $solrfield_MIMETYPE = 'fedora_datastream_version_' . $dsid . '_MIMETYPE_ms';
    $solrfield_SIZE = 'fedora_datastream_version_' . $dsid . '_SIZE_ms';
    if (isset($solrRecord[$solrfield_ID]) && isset($solrRecord[$solrfield_LABEL]) && isset($solrRecord[$solrfield_CREATED]) && isset($solrRecord[$solrfield_MIMETYPE]) && isset($solrRecord[$solrfield_SIZE])) {
      foreach ($solrRecord[$solrfield_CREATED] as $key => $CREATED) {
        $pre = ((1+$key) == count($solrRecord[$solrfield_CREATED])) ? '<b title="current version">' : '';
        $post = ((1+$key) == count($solrRecord[$solrfield_CREATED])) ? '</b>' : '';
        $rows[$CREATED] = array('ID' => ($pre) ? $pre . $dsid . $post : '',
          'version ID' => $pre . $solrRecord[$solrfield_ID][$key] . $post,
          'LABEL' => $pre . $solrRecord[$solrfield_LABEL][$key] . $post,
          'CREATED' => $pre . $solrRecord[$solrfield_CREATED][$key] . $post,
          'MIMETYPE' => $pre . $solrRecord[$solrfield_MIMETYPE][$key] . $post,
          'SIZE' => array('data' => $pre . number_format($solrRecord[$solrfield_SIZE][$key]) . $post, 'class' => array('numeric')),
          'Operations' => $pre . l('view', '/islandora/object/' . urlencode($pid) . '/datastream/' . $dsid . '/version/' . $key . '/view') . ' &nbsp; &nbsp; ' . l('revert', '/islandora/object/' . urlencode($pid) . '/datastream/' . $dsid . '/version/' . $key . '/revert') . $post,
        );
      }
    }
    else {
      $messages[] = '<div class="message warning">One of the Solr fields is not set for datatream ' . $dsid . '.  ' . l(t('Manage datastreams'), '/islandora/object/' . urlencode($pid) . '/manage/datastreams'). '.</div>';
    }
  }

  ksort($rows);
  return implode($messages) . theme('table', array('rows'=>$rows,'header'=>$headings));
}

function upitt_workflow_lookup_audit_record($PID) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $tmp_parts = explode(":", $PID);
  $record_str = '';
  if (count($tmp_parts) > 0) {
    $barcode = str_replace($tmp_parts[0] . ':', '', $PID);

    $batch_detail = 'SELECT bi.batch_id, i.do_id `item.do_id`, b.batch_external_id, b.batch_description, b.item_count `Total Objects in Batch`, ' .
                    '  (SELECT COUNT(itf.id) FROM item_file itf WHERE itf.item_id = bi.item_id AND itf.`use` = \'MASTER\') as `Items in this object` ' .
                    'FROM item i ' .
                    '  JOIN batch_item bi ON (bi.item_id = i.id) ' .
                    '  JOIN batch b ON (bi.batch_id = b.batch_id) ' .
                    'WHERE i.do_id = \'' . mysqli_real_escape_string($link, $barcode) . '\' ' . 
                    'LIMIT 1';
    $batch = array();
    $result = mysqli_query($link, $batch_detail);
    if (!$result) {
      $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
      $message .= 'Whole query: ' . $batch_detail;
      die($message);
    }

    $fields = array();
    if ($row = mysqli_fetch_assoc($result)) {
      $item_id = $row['item.do_id'];
      $batch_item_count = $row['Total Objects in Batch'];
      $object_item_count = $row['Items in this object'];
      $batch_pkey_id = $row['batch_id'];
      $fields = array_keys($row);
      foreach ($fields as $field) {
        if ($field == 'batch_id') {
          $record_str .= '<b>' . $field . '</b> ' . l($row[$field], '/workflow/batch/' . $row[$field]) . '<br>';
        }
        elseif ($field == 'item.do_id') {
          $record_str .= '<b>' . $field . '</b> ' . l($row[$field], '/workflow/object/' . $row[$field]) . '<br>';
        }
        elseif ($field == 'batch_external_id') {
          $record_str .= '<b>' . $field . '</b> ' . l($row[$field], '/workflow/batch/' . $row['batch_id']) . '<br>';
        }
        else {
          $record_str .= '<b>' . $field . '</b> ' . $row[$field] . '<br>';
        }
      }
    }
    mysqli_close($link);
  }
  return array('batch_id' => $batch_pkey_id, 'batch_item_count' => $batch_item_count, 'object_item_count' => $object_item_count, 'item.do_id' => $item_id, 'markup' => $record_str);
}

function upitt_workflow_lookup_object_items($batch_id, $item_do_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $object_items = 'SELECT itf.`name`, itf.timestamp ' .
                  'FROM batch_item bi ' .
                  '  JOIN item_file itf ON (itf.item_id = bi.item_id AND itf.`use` = \'MASTER\') ' .
                  '  JOIN item i ON (i.id = bi.item_id) ' .
                  'WHERE bi.batch_id = ' . $batch_id . ' AND i.do_id = ' . $item_do_id;
dpm($object_items);
  $batch = array();
  $result = mysqli_query($link, $object_items);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $object_items;
    die($message);
  }

  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[] = $row;
  }
  return $rows;
}

function upitt_workflow_audit_analyze_pagecount_mismatch($audit_record, $child_pages, $pid) {
  $object_items = upitt_workflow_lookup_object_items($audit_record['batch_id'], $audit_record['item.do_id']);
  if (count($object_items) > 0) {
    $object_items_headings = array_keys($object_items[0]);
    $object_items_headings[] = 'Matching Islandora Object';
    dpm($child_pages);
    dpm($object_items);
    foreach ($object_items as $key => $row) {
      $name_parts = explode('.', $row['name']);
      if (strstr($row['name'], '.')) {
        array_pop($name_parts);
      }
      $page_pid = $pid . '-' . implode('.', $name_parts);
      // try to load this pid as an islandora_object
      $page_object = islandora_object_load($page_pid);
      $object_items[$key]['Matching Islandora Object'] = (is_object($page_object)) ? l($page_object->label, '/islandora/object/' . $page_pid . '/manage') : '<b style="color:red">not found</b>';
    }  
    return theme('table', array('rows' => $object_items, 'header' => $object_items_headings));
  }
  else {
    return '';
  }
}
