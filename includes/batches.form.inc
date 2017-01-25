<?php

function upitt_workflow_batches_form(array $form, array &$form_state) {
  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  
  $item = menu_get_item();
  // Determine whether or not this is for "All" or "Active" batches
  $is_all = ($item['path'] == 'workflow/batches/all');

  $batch_records = 'SELECT ' .
                   ' b.batch_id, ' .
                   ' IF(b.is_batch_active = 1, \'Yes\', \'No\') `active`, ' .
                   ' b.batch_priority `priority`, ' .
                   ' b.batch_external_id `name`, ' .
                   ' b.date `date`, ' .
                   ' c.c_id `collection`, ' .
                   ' bit.name `content type`, ' . 
//                   ' ct.content_name `content type`, ' .
                   ' po.name `prop. owner`, ' .
                   ' b.item_count `items`, ' .
                   ' b.batch_description `notes`, ' .
                   ' cws.name `workflow`, ' .
                   ' "PENDING NEW status TABLE" `status` ' .
                   'FROM batch b ' .
                   'LEFT OUTER JOIN `workflow_sequence` cws ON (cws.id = b.batch_sequence_id) ' .
                   'JOIN `batch_item_type` bit ON (bit.id = b.type_id) ' .
//                   'LEFT OUTER JOIN `content_types` ct ON (ct.content_type_id = b.content_type_id) ' .
                   'JOIN `collection` c ON (c.id = b.mapto_collections) ' .
                   'JOIN `property_owner` po ON (po.property_owner_id = b.batch_property_owner_id) ' .
                   (($is_all) ? '' : 'WHERE b.is_batch_active = 1 ') .
                   'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;

  $query = 'SELECT count(batch_external_id) as `total` ' .
           'FROM batch b ' .
           (($is_all) ? '' : 'WHERE b.is_batch_active = 1 ');

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);

  $result = mysqli_query($link, $batch_records);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $batch_records;
    die($message);
  }

  $batch_records_headings = $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['name']) && isset($row['batch_id'])) {
      $row['name'] = l($row['name'], '/workflow/batch/' . $row['batch_id']);
      unset($row['batch_id']);
    }
    // Now that the row has been adjusted to remove the "hidden" column that was used to create the link, make the column headings from the array_keys of the row.
    if (count($batch_records_headings) < 1) {
      $batch_records_headings = array_keys($row);
    }
    $rows[] = $row;
  }

  $form = array();
  $links = array(l('All Objects', '/workflow'),
                 l('Active Batches', '/workflow/batches/active'),
                 l('All Batches', '/workflow/batches/all'),
           );

  mysqli_close($link);
  $pager = theme('pager', array('quantity',$count_rows));
  $table_markup = theme('upitt_workflow_batches',
      array('table' => $pager . theme('table', array('rows'=>$rows,'header'=>$batch_records_headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'links' => $links,
            'count_rows' => number_format($count_rows),
            'post_to' => '/workflow',
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);

  return $form;
}

