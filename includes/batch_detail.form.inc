<?php

function upitt_workflow_batch_detail_form($batch_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $batch_detail = 'SELECT b.batch_id, b.nid, b.batch_external_id `name`, b.date `date`, b.mapto_collections `collection/s`, ws.name `workflow` ' .
                  'FROM batch b ' .
                  'JOIN workflow_sequence ws ON (ws.id = b.batch_sequence_id) ' .
                  'WHERE b.batch_id = \'' . $batch_id . '\' ';

  $batch_items = 'SELECT "" as `ingested?`, i.do_id, i.`name`, GROUP_CONCAT(ca.`name`) `required actions` ' .
//                 '  (SELECT `name` FROM `action` WHERE id = cwsa.action_id) `actions` ' .
                 'FROM batch b ' .
                 'JOIN workflow_sequence_actions cwsa ON (cwsa.workflow_sequence_id = b.batch_sequence_id) ' .
                 'JOIN `action` ca ON (ca.id = cwsa.action_id) ' .
                 'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
                 'JOIN item i ON (bi.item_id = i.id) ' .
                 'WHERE b.batch_id = \'' . $batch_id . '\' ' .
                 'GROUP BY i.id ORDER BY i.id ASC, cwsa.`order` ASC ' .
                 'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;
dpm($batch_items);
  $query = 'SELECT count(i.do_id) as `total` ' .
           'FROM batch b ' .
           'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
           'JOIN item i ON (bi.item_id = i.id) ' .
           'WHERE b.batch_id = \'' . $batch_id . '\'';

  $result = mysqli_query($link, $query);

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  $count_rows_markup = number_format($count_rows) . " workflow batch items " . (($page <> 0) ? " (page " . $page . " of " . round($count_rows / WF_OBJECTS_PER_PAGE) . ")" : "");

  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);

  $collections = upitt_workflow_lookup_collection_names();

  $result = mysqli_query($link, $batch_detail);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $batch_detail;
    die($message);
  }

  $title = 'unknown batch';
  while ($row = mysqli_fetch_assoc($result)) {
    $title = (isset($row['name']) ? $row['name'] : $title);
    if (isset($row['batch_id']) && isset($row['batch'])) {
      $row['batch'] = l($row['batch'], '/workflow/batch/' . $row['batch_id']);
      unset($row['batch_id']);
    }
    else {
      $row['batch'] = 'n/a';
    }
    if (isset($row['nid'])) {
      if ($row['nid'] > 0) {
        $row['Drupal node'] = l($title, '/node/' . $row['nid']);
      }
      unset($row['nid']);
    }
    if (isset($row['collection/s'])) { 
      // this has to lookup the collection names from the array that is loaded before this loop.
      $row['collection/s'] = upitt_workflow_collection_id_map_names($row['collection/s'], $collections);
    }
    $details = theme('workflow_item_details', array('details' => $row));
  }

  $result = mysqli_query($link, $batch_items);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $batch_items;
    die($message);
  }

  $batch_item_headings = $batch_rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($batch_item_headings) < 1) {
      $batch_item_headings = array_keys($row);
    }
    if (array_key_exists('do_id', $row)) {
      $pid = 'pitt:' . $row['do_id'];
      if ($obj = islandora_object_load($pid)) {
        $row['ingested?'] = l($row['do_id'], '/islandora/object/' . $pid, array('attributes' => array('target' => '_blank')));
      } else {
        $row['ingested?'] = '';
      }
      $row['do_id'] = l($row['do_id'], '/workflow/object/' . $row['do_id']);
    }
    $batch_rows[] = $row;
  }
  mysqli_close($link);
  $pager = theme('pager', array('quantity',$count_rows));
  $table_markup = theme('upitt_workflow_report',
      array('table' => $pager . theme('table', array('rows'=>$batch_rows,'header'=>$batch_item_headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'count_rows' => $count_rows_markup,
            'links' => array(l('Edit', '/workflow/batch/edit/' . $batch_id)),
            'post_to' => '/workflow',
            'collection_filter_choices' => upitt_workflow_get_collection_choices(),
            'details' => $details
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);

  return $form;
}

