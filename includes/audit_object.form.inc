<?php

function upitt_workflow_object_manage_audit($islandora_object) { 
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  if (is_object($islandora_object)) {
    $audit_record = upitt_workflow_lookup_audit_record($islandora_object->id);
    $solr_record = upitt_workflow_get_solr_record($islandora_object->id);    
    $solr_record_analysis_table_markup = upitt_workflow_analyze_solr_record($solr_record, $islandora_object->id);

    // check against the "all_ingests.txt" file
    $all_ingests_file = file(drupal_get_path('module', 'upitt_workflow') . '/includes/all_ingests.txt');
    @list($namespace, $id_no_namespace) = explode(":", $islandora_object->id);
    $found = FALSE;
    foreach ($all_ingests_file as $l) {
      if (!$found) {
        $found |= (trim($l) == $id_no_namespace);
      }
    }
//    $found = (!(array_search($id_no_namespace, $all_ingests_file) === FALSE));

    $markup = '<h3>Object Audit</h3> ' .
              '<b>Islandora Object PID:</b> ' . $islandora_object->id . '<br>' . 
              '<b>Model:</b>' . implode(', ', $islandora_object->models) . '<br>';              
    $markup .= '<h4>Object "' . $id_no_namespace . '" ingested upstairs?</h4>' .
              '<p>' . l($id_no_namespace, '/workflow/object/' . $id_no_namespace) . ' is ' . ($found ? '' : 'not') . ' in the "all_ingests.txt" file.</p>';
    if (isset($audit_record['markup']) && $audit_record['batch_item_count'] > 0) {
      $markup .= '<h4>Workflow Values for "' . $islandora_object->id . '"</h4>' .
                 $audit_record['markup'] . '<hr>' .
                 '<p>Based on the records for this object in the Workflow system, there should be the following number of objects.</p>' .
                 '<div style="color:#934">' . l(number_format($audit_record['batch_item_count']) . ' total objects in this batch', '/workflow/batch/' . $audit_record['batch_id']) . '</div>' .
                 '<div style="color:#934">' . number_format($audit_record['object_item_count']) . ' items in this object' . '</div>';
    }
    else {
      $markup .= '<h4>No Workflow records for "' . $islandora_object->id . '"' . '</h4>';
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
  $headings = array(
    array('data' => 'ID', 'field' => 'ID', 'sort' => 'ASC'),
    array('data' => 'Label', 'field' => 'LABEL', 'sort' => 'ASC'),
    array('data' => 'Created', 'field' => 'CREATED', 'sort' => 'ASC'),
    array('data' => 'Mimetype', 'field' => 'MIMETYPE', 'sort' => 'ASC'),
    array('data' => 'Size', 'field' => 'SIZE', 'sort' => 'ASC'),
    array('data' => 'Operations', 'field' => 'Operations'),
  );
  
  $messages = array();
  foreach ($datastreams as $dsid) {
    $solrfield_ID = 'fedora_datastream_version_' . $dsid . '_ID_ms';
    $solrfield_LABEL = 'fedora_datastream_version_' . $dsid . '_LABEL_ms';
    $solrfield_CREATED = 'fedora_datastream_version_' . $dsid . '_CREATED_ms';
    $solrfield_MIMETYPE = 'fedora_datastream_version_' . $dsid . '_MIMETYPE_ms';
    $solrfield_SIZE = 'fedora_datastream_version_' . $dsid . '_SIZE_ms';
    if (isset($solrRecord[$solrfield_ID]) && isset($solrRecord[$solrfield_LABEL]) && isset($solrRecord[$solrfield_CREATED]) && isset($solrRecord[$solrfield_MIMETYPE]) && isset($solrRecord[$solrfield_SIZE])) {
      foreach ($solrRecord[$solrfield_CREATED] as $key => $CREATED) {
        $rows[$CREATED] = array('ID' => $solrRecord[$solrfield_ID][$key],
          'LABEL' => $solrRecord[$solrfield_LABEL][$key],
          'CREATED' => $solrRecord[$solrfield_CREATED][$key],
          'MIMETYPE' => $solrRecord[$solrfield_MIMETYPE][$key],
          'SIZE' => array('data' => number_format($solrRecord[$solrfield_SIZE][$key]), 'class' => array('numeric')),
          'Operations' => l('view', '/islandora/object/' . urlencode($pid) . '/datastream/' . $dsid . '/version/' . $key . '/view') . ' &nbsp; &nbsp; ' . l('revert', '/islandora/object/' . urlencode($pid) . '/datastream/' . $dsid . '/version/' . $key . '/revert'),
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
  return array('batch_id' => $batch_pkey_id, 'batch_item_count' => $batch_item_count, 'object_item_count' => $object_item_count, 'markup' => $record_str);
}
