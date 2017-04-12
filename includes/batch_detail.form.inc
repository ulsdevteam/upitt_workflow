<?php

function upitt_workflow_batch_detail_form($batch_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $where_batch_clause = ((is_numeric($batch_id)) ? 'WHERE b.batch_id = \'' . $batch_id . '\' ' :
    'WHERE b.batch_external_id = \'' . $batch_id . '\' ');


  $batch_detail = 'SELECT b.batch_id, b.nid, b.batch_external_id `name`, b.date `date`, b.mapto_collections `collection/s`, ws.name `workflow`, b.file ' .
                  'FROM batch b ' .
                  'LEFT OUTER JOIN workflow_sequence ws ON (ws.id = b.batch_sequence_id) ' .
                  $where_batch_clause;

  $batch_items = 'SELECT "" as `ingested?`, i.do_id, i.`name`, IFNULL(GROUP_CONCAT(ca.`name`), \'\') `required actions` ' .
                 'FROM batch b ' .
                 'LEFT OUTER JOIN workflow_sequence_actions cwsa ON (cwsa.workflow_sequence_id = b.batch_sequence_id) ' .
                 'LEFT OUTER JOIN `action` ca ON (ca.id = cwsa.action_id) ' .
                 'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
                 'JOIN item i ON (bi.item_id = i.id) ' .
                 $where_batch_clause . 
                 'GROUP BY i.id ORDER BY i.id ASC, cwsa.`order` ASC ' .
                 'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;

  $query = 'SELECT count(i.do_id) as `total` ' .
           'FROM batch b ' .
           'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
           'JOIN item i ON (bi.item_id = i.id) ' .
           $where_batch_clause;

  $result = mysqli_query($link, $query);

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  $ob1_page = $page + 1;
  $count_rows_markup = number_format($count_rows) . " workflow batch items " . (($ob1_page <> 0) ? " (page " . $ob1_page . " of " . round($count_rows / WF_OBJECTS_PER_PAGE) . ")" : "");

  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);

  $collections = upitt_workflow_lookup_collection_names();

  $result = mysqli_query($link, $batch_detail);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $batch_detail);
  }

  $details = '';
  $title = 'unknown batch';
  $file_rows = $file_header = array();

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

    $arr = upitt_workflow_csv_file_as_array($row['file']);
    $file_header = (isset($arr['header']) ? $arr['header'] : array());
    $file_rows = (isset($arr['rows']) ? $arr['rows'] : array());
    $details = (count($file_rows) < 1) ?
      '<table><tr><td>There is no CSV file associated with this batch.  ' . l(t('Edit'), '/workflow/batch/edit/' . $batch_id, array('fragment' => 'file_upload')) . ' this batch to upload a CSV file.</td></tr></table>'
      : theme('table', array('rows' => $file_rows, 'header' => $file_header, 'attributes' => array('class' => array('report_table max_height_500'))));
       // theme('workflow_item_details', array('details' => $row));
  }

  $result = mysqli_query($link, $batch_items);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $batch_items);
  }

  $ingest_namespace = variable_get('upitt_workflow_ingest_namespace', 'pitt:');
  $batch_item_headings = $batch_rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($batch_item_headings) < 1) {
      $batch_item_headings = array_keys($row);
    }
    if (array_key_exists('do_id', $row)) {
      $pid = $ingest_namespace . $row['do_id'];
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
  $links = array(l('Edit', '/workflow/batch/edit/' . $batch_id),
                 l('Islandora Ingest', '/workflow/batch/' . $batch_id . '/islandora_ingest'));
  // If the islandora_datastreams_io module is installed and the user has permission, create the export link.
  $export_link = ((module_exists('islandora_datastreams_io') && user_access(ISLANDORA_DATASTREAMS_IO)) ?
    l('Export', '/workflow/datastreams_io/export/batch/' . $batch_id) : '');
  if ($export_link) {
    $links[] = $export_link;
  }
  $table_markup = theme('upitt_workflow_report',
      array('table' => $details, //$pager . theme('table', array('rows'=>$batch_rows,'header'=>$batch_item_headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'count_rows' => $count_rows_markup,
            'links' => $links,
            'post_to' => '/workflow',
            'collection_filter_choices' => upitt_workflow_get_collection_choices(),
            'details' => '', // $details
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);
  return $form;
}

