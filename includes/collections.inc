<?php

function upitt_workflow_collections_form($c_id = '') {
  list($count_rows_markup, $table_markup) = ($c_id <> '') ? upitt_workflow_collections_get_c_id_table($c_id) : upitt_workflow_collections_get_table();

  $links = array(l('Active Batches', '/workflow/batches/active'),
                 l('All Batches', '/workflow/batches/all') );
  $breadcrumb = upitt_workflow_get_breadcrumb_path();

  $page['#markup'] = theme('upitt_workflow_collection_page', array('links' => $links, 'breadcrumb' => $breadcrumb, 'count_rows' => $count_rows_markup, 'table' => $table_markup));
  return $page;
}

function upitt_workflow_collections_get_table() {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $collection_records = 'SELECT c.c_id, c.name `collection`, ' .
                        '  (SELECT IFNULL(SUM(b.item_count), 0) FROM batch_collections bc JOIN batch b ON (b.batch_id = bc.batch_id) WHERE bc.collection_id = c.id) as `items`, ' .
                        '  (SELECT COUNT(bc.batch_id) FROM batch_collections bc WHERE bc.collection_id = c.id) as `batch(es)`, ' .
                        '  "PENDING RESEARCH" as `status` ' .
                        'FROM collection c ' .
                        'ORDER BY c.c_id';

  $query = 'SELECT count(*) as `total` ' .
           'FROM collection';
  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $result = mysqli_query($link, $collection_records);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $collection_records);
  }

  $collection_records_headings = $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    // recalulate the contents of the collection field based off of the collection field ALONG WITH The c_id value.
    $row['collection'] = '<b>' . $row['c_id'] . '</b> <small>' . $row['collection'] . '</small>';

    $row['items'] = array('data' => l(number_format($row['items']), '/workflow/collection/' . urlencode($row['c_id'])), 'class' => 'numeric');
    $row['batch(es)'] = array('data' => number_format($row['batch(es)']), 'class' => 'numeric');


    // Finally, can remove this column
    unset($row['c_id']);

    // Make the column headings from the array_keys of the row.
    if (count($collection_records_headings) < 1) {
      $collection_records_headings = array_keys($row);
    }
    $rows[] = $row;
  }

  $count_rows_markup = "<h5>" . number_format($count_rows) . " collections</h5>";
  $table_markup = theme('table', array('rows' => $rows, 'header' => $collection_records_headings, 'attributes' => array('class' => array('report_table'))));
  mysqli_close($link);

  return array($count_rows_markup, $table_markup);
}

function upitt_workflow_collections_get_c_id_table($c_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $collection_records = 'SELECT i.do_id `item id`, ' .
                        '  i.name, b.batch_external_id, ' .
                        '  "" `ready for`, ' .
                        '  (SELECT ta.description ' .
                        '   FROM `transaction` t ' .
                        '   JOIN `transaction_actions` ta ON (ta.id = t.transaction_action_id) ' .
                        '   WHERE t.item_id = i.id ' .
                        '   ORDER BY t.timestamp DESC ' .
                        '   LIMIT 1) `last action`, ' .
                        '  (SELECT t.timestamp ' .
                        '   FROM `transaction` t ' .
                        '   WHERE t.item_id = i.id ' .
                        '   ORDER BY t.timestamp DESC ' .
                        '   LIMIT 1) `when`, ' .
                        '  (SELECT lau.username ' . 
                        '   FROM `transaction` t ' . 
                        '   JOIN `legacy_auth_user` lau ON (lau.id = t.user_id) ' . 
                        '   WHERE t.item_id = i.id ' . 
                        '   ORDER BY t.timestamp DESC ' . 
                        '   LIMIT 1) `who`, ' . 
                        '    (SELECT CONCAT(" [", COUNT(s_itf.name), " files], ", GROUP_CONCAT(s_itf.name)) ' .
                        '    FROM item_file s_itf ' . 
                        '    WHERE s_itf.item_id = i.id) `files` ' .
                        'FROM collection c ' .
                        'JOIN batch_collections bc ON (bc.collection_id = c.id) ' .
                        'JOIN batch b ON (bc.batch_id = b.batch_id) ' .
                        'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
                        'JOIN item i ON (i.id = bi.item_id) ' .
                        'JOIN item_file itf ON (itf.item_id = bi.item_id AND itf.`use` = \'MODS\') ' . 
                        'WHERE c.c_id = \'' . mysqli_real_escape_string($link, $c_id) . '\' ' .
                        'ORDER BY itf.timestamp ' .
                        'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;
dpm($collection_records);
  // perhaps this query can just return the item_count field value at the branch level, but this query is the actual count - so it should be more accurate (but slower) to query the table this way.
  $query = 'SELECT count(itf.name) as `total` ' .
           'FROM collection c ' . 
           'JOIN batch_collections bc ON (bc.collection_id = c.id) ' . 
           'JOIN batch b ON (bc.batch_id = b.batch_id) ' .
           'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
           'JOIN item i ON (i.id = bi.item_id) ' .
           'JOIN item_file itf ON (itf.item_id = bi.item_id AND itf.`use` = \'MODS\') ' .
           'WHERE c.c_id = \'' . mysqli_real_escape_string($link, $c_id) . '\'';

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);

  $result = mysqli_query($link, $collection_records);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $collection_records);
  }

  $collection_records_headings = $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    // Make the column headings from the array_keys of the row.
    if (count($collection_records_headings) < 1) {
      $collection_records_headings = array_keys($row);
    }
    $row['item id'] = l($row['item id'], '/workflow/object/' . $row['item id']);
    $rows[] = $row;
  }

  $count_rows_markup = "<h5>" . number_format($count_rows) . " items</h5>";
  $pager = theme('pager', array('quantity', $count_rows));

  $table_markup = $pager . theme('table', array('rows' => $rows, 'header' => $collection_records_headings, 'attributes' => array('class' => array('report_table')))) . $pager;
  mysqli_close($link);

  return array($count_rows_markup, $table_markup);
}
