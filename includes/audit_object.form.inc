<?php

function upitt_workflow_object_manage_audit($islandora_object) { 
  module_load_include('inc', 'islandora', 'includes/utilities');

  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');

  $workflow_page_record_exists = FALSE;
  if (is_object($islandora_object)) {
    $is_paged = upitt_workflow_is_paged_object($islandora_object);

    $audit_record = upitt_workflow_lookup_audit_record($islandora_object->id);
    $datastream_sizes_objs = upitt_workflow_get_depositor_model_and_datastreams_sizes($islandora_object->id);
    $dsid_sizes = array();
    foreach ($datastream_sizes_objs as $solr_fieldname => $datastream_sizes) {
      $dsid = str_replace(array('fedora_datastream_version_', '_SIZE_ms', '_SIZE_i'), "", $solr_fieldname);
      $dsid_sizes[$dsid] = 0;
      if (is_array($datastream_sizes)) {
        // Multistring SIZE field
        foreach ($datastream_sizes as $datastream_size) {
          $dsid_sizes[$dsid] += $datastream_size;
        }
      }
      else {
        // Integer SIZE field
        $dsid_sizes[$dsid] += $datastream_sizes;
      }
    }

    $solr_record = upitt_workflow_get_solr_record($islandora_object->id);    
    $solr_record_analysis_table_markup = upitt_workflow_analyze_solr_record($solr_record, $islandora_object->id);

    @list($namespace, $id_no_namespace) = explode(":", $islandora_object->id);

    // check against the "all_ingests.txt" file
    $found = upitt_workflow_check_for_object_in_all_ingests($id_no_namespace);

    // For a paged object, just look for the pid value ($id_no_namespace) in the all_intests file, but for a 
    // page or manuscript page the parent object should be on the all_ingests list.
    if (!$is_paged) {
      $workflow_page_record_exists = upitt_workflow_does_workflow_page_record_exist($id_no_namespace);
    }

    $markup = '<h3>Object Audit' .
              ((module_exists('upitt_islandora_inventory_audit')) ? ' ' . l('Details', 'islandora/object/'.$islandora_object->id.'/manage/audit_upitt_workflow/inventory_audit') : '') .
              '</h3> ' .
              '<b>Islandora Object PID:</b> ' . $islandora_object->id . '<br>';              
    $markup .= '<h4>Object "' . $id_no_namespace . '" ingested upstairs?</h4>' .
              '<p>' . l($id_no_namespace, '/workflow/object/' . $islandora_object->id) . ' is ' . ($found ? '' : 'not') . ' in the "all_ingests.txt" file. ' .
              (($is_paged) ? '' : 'This page object (' . $islandora_object->id . ') ' . (($workflow_page_record_exists) ? 'has' : 'DOES NOT HAVE') . ' a matching record in the workflow database') . 
              '</p>';
    if (isset($audit_record['markup']) && $audit_record['batch_item_count'] > 0) {
      $markup .= '<h4>Workflow Values for "' . $islandora_object->id . '" / batch item_id = "' . $audit_record['item.do_id'] . '".</h4>' .
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

    if (count($dsid_sizes) > 0) {
      $markup .= '<h4>Datastream File Size Report</h4><p>This is the total size for all datastreams on this object -- including all versions for each datastream.</p>';
      $grand_total_datastreams_size = 0;
      foreach ($dsid_sizes as $dsid => $total_size) {
        if ($dsid <> 'RELS_EXT_hasModel_uri_s') {
          $grand_total_datastreams_size += $total_size;
          $markup .= '<p><b>' . $dsid . '</b> size: ' . islandora_convert_bytes_to_human_readable($total_size) . '</p>';
        }
      }
      $markup .= '<hr><p><b>Grand TOTAL size</b>: ' . islandora_convert_bytes_to_human_readable($grand_total_datastreams_size) . '</p>';
    }

    $markup .= ($solr_record_analysis_table_markup) ? '<hr><h4>Datastream history</h4>' . $solr_record_analysis_table_markup : '';
    return $markup;
  }
}

function upitt_workflow_lookup_audit_record($PID) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $tmp_parts = explode(":", $PID);
  $record_str = '';
  // Set up this value in case the record does not exist.  This will likely be
  // updated below before the value is passed back to the item.form code
  $item_id = 'no record exists in {islandora_workflow.batch} table for this item.';
  $object_item_count = $batch_item_count = $batch_pkey_id = 0;
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
      upitt_workflow_sql_error_die($link, $batch_detail);
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
  return array('batch_id' => $batch_pkey_id, 
               'batch_item_count' => $batch_item_count,
               'object_item_count' => $object_item_count,
               'item.do_id' => $item_id,
               'markup' => $record_str);
}

function upitt_workflow_lookup_object_items($batch_id, $item_do_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $object_items = 'SELECT itf.`name`, itf.timestamp ' .
                  'FROM batch_item bi ' .
                  '  JOIN item_file itf ON (itf.item_id = bi.item_id AND itf.`use` = \'MASTER\') ' .
                  '  JOIN item i ON (i.id = bi.item_id) ' .
                  'WHERE bi.batch_id = ' . $batch_id . ' AND i.do_id = ' . $item_do_id;

  $batch = array();
  $result = mysqli_query($link, $object_items);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $object_items);
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
