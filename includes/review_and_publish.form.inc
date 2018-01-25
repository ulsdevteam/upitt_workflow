<?php

function upitt_workflow_review_and_publish(array $form, array &$form_state) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $records = 'SELECT b.batch_external_id `object/batch`, b.batch_id, ' .
             '  (SELECT MAX(t_s.timestamp) ' .
             '   FROM item_current_status ics ' .
             '   JOIN `transaction` t_s ON (t_s.id = ics.last_transaction_id) ' .
             '   WHERE ics.item_id = bi.item_id) as `date of last transaction`, ' .
             '\'\' `operation` ' .
             'FROM batch b ' .
             'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
             'JOIN `transaction` t ON (t.item_id = bi.item_id AND t.transaction_action_id = ' . UPITT_WORKFLOW_ACTION_INGESTED_COMPLETELY . ') ' .
             'LEFT OUTER JOIN `transaction` t2 ON (t2.item_id = bi.item_id AND t2.transaction_action_id = ' . UPITT_WORKFLOW_ACTION_OBJECT_PUBLISHED_ON_SITE . ') ' .
             'WHERE t2.id IS NULL ' .
             'GROUP BY b.batch_id ' .
             'ORDER BY `date of last transaction` DESC ' .
             'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;

  $query = 'SELECT count(distinct b.batch_external_id) as `total` ' .
           'FROM batch b ' .
           'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
           'JOIN `transaction` t ON (t.item_id = bi.item_id AND t.transaction_action_id = ' . UPITT_WORKFLOW_ACTION_INGESTED_COMPLETELY . ') ' .
           'LEFT OUTER JOIN `transaction` t2 ON (t2.item_id = bi.item_id AND t2.transaction_action_id = ' . UPITT_WORKFLOW_ACTION_OBJECT_PUBLISHED_ON_SITE . ') ' .
           'WHERE t2.id IS NULL';

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);

  $result = mysqli_query($link, $records);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $records);
  }

  $records_headings = $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $batch_external_id = $row['object/batch'];
    $row['object/batch'] = l($row['object/batch'], '/workflow/batch/' . $row['batch_id']);
    unset($row['batch_id']);
    // Now that the row has been adjusted to remove the "hidden" column that was used to create the link, make the column headings from the array_keys of the row.
    if (count($records_headings) < 1) {
      $records_headings = array_keys($row);
    }
    $ingest_namespace = upitt_workflow_get_ingest_namespace();
    $pid = $ingest_namespace . $batch_external_id;

    // create the links to review and publish for the `operation` field
    $test_object = islandora_object_load($pid);
    $review = (!is_object($test_object)) ? '<span class="error">object missing</span>' : l('Review', '/islandora/object/' . $pid);

    $publish = (!is_object($test_object)) ? '<span class="error">object missing</span>' : l('Publish', '/workflow/object/' . $batch_external_id . '/do_publish');
    $row['operation'] = array('data' => $review . ' | ' . $publish);

    $rows[] = $row;
  }

  $form = array();
  $links = upitt_workflow_get_basic_links();

  mysqli_close($link);
  $ob1_page = $page + 1;
  $count_rows_markup = number_format($count_rows) . " batches " . (($ob1_page <> 0) ? " (page " . $ob1_page . " of " . round($count_rows / WF_OBJECTS_PER_PAGE) . ")" : "");

  $pager = theme('pager', array('quantity', $count_rows));
  $table_markup = theme('upitt_workflow_batches',
      array('table' => $pager . theme('table', array('rows'=>$rows,'header'=>$records_headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'links' => $links,
            'breadcrumb' => upitt_workflow_get_breadcrumb_path(),
            'count_rows' => $count_rows_markup,
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);

  return $form;
}

