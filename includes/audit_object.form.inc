<?php

function upitt_workflow_object_manage_audit($islandora_object) { 
  if (is_object($islandora_object)) {
    $audit_record = upitt_workflow_lookup_audit_record($islandora_object->id);
    
    $markup = '<h3>Object Audit</h3> ' .
              '<b>Islandora Object PID:</b> ' . $islandora_object->id . '<br>' . 
              '<b>Model:</b>' . implode(', ', $islandora_object->models) . '<br>';
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
    return $markup;
  }
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
        $record_str .= '<b>' . $field . '</b> ' . $row[$field] . '<br>';
      }
    }
    mysqli_close($link);
  }
  return array('batch_id' => $batch_pkey_id, 'batch_item_count' => $batch_item_count, 'object_item_count' => $object_item_count, 'markup' => $record_str);
}
