<?php

function upitt_workflow_collections_form() {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $collection_records = 'SELECT * ' .
                        'FROM collection c';

  $query = 'SELECT count(*) as `total` ' .
           'FROM collection';
  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $result = mysqli_query($link, $collection_records);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $collection_records;
    die($message);
  }

  $collection_records_headings = $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['name']) && isset($row['batch_id'])) {
      $row['name'] = l($row['name'], '/workflow/batch/' . $row['batch_id']);
      unset($row['batch_id']);
    }
    // Now that the row has been adjusted to remove the "hidden" column that was used to create the link, make the column headings from the array_keys of the row.
    if (count($collection_records_headings) < 1) {
      $collection_records_headings = array_keys($row);
    }
    $rows[] = $row;
  }

  $form = array();
  $links = array(l('Active Batches', '/workflow/batches/active'),
                 l('All Batches', '/workflow/batches/all') );
  mysqli_close($link);

  $count_rows_markup = "<h5>" . number_format($count_rows) . " collections</h5>";
  $table_markup = theme('table', array('rows' => $rows, 'header' => $collection_records_headings, 'attributes' => array('class' => array('report_table'))));

  return $count_rows_markup . "<br>" . $table_markup;
}
