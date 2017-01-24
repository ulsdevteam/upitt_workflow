<?php

function upitt_workflow_objects() {
  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  // if there is any selection of the filters, then the data must be filtered with a WHERE clause, else don't add that condition
  $where_clause = '';

  if (isset($_GET['batch_external_id_filter']) && $_GET['batch_external_id_filter'] <> '') {
    $where_clause .= "ci.name like '%" . upitt_workflow_safe_qstring($_GET['batch_external_id_filter']) . "%' ";
  }

//XX
/* username from legacy system linked to auth_user table which will NOT be part of new system
  if (isset($_GET['username_filter']) && $_GET['username_filter'] <> '') {
    $where_clause .= ($where_clause ? ' AND ' : '') . "po.username like '%" . upitt_workflow_safe_qstring($_GET['username_filter']) . "%' ";
  }
*/
  if (isset($_GET['type_filter']) && $_GET['type_filter'] <> '') {
    $where_clause .= ($where_clause ? ' AND ' : '') . "cit.name = '" . upitt_workflow_safe_qstring($_GET['type_filter']) . "' ";
  }
//XX
/*  if (isset($_GET['collection_filter']) && $_GET['collection_filter'] <> '') {
    $where_clause .= ($where_clause ? ' AND ' : '') . "cc.name = '" . upitt_workflow_safe_qstring($_GET['collection_filter']) . "' ";
  }
*/
  $where_clause = ($where_clause) ? "WHERE " . $where_clause : ' ';

  $query = 'SELECT count(do_id) `total` ' .
           'FROM `item` ci ' .
//XX           'JOIN auth_user po ON (po.id = ci.property_owner_id) ' .
           'JOIN item_type cit ON (cit.id = ci.type_id) ' .
           'JOIN collection cc ON (cc.id = ci.primary_collection_id) ' .
           $where_clause;

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT); // mysql_query($query, $link);
  if (!$result) {
     echo(mysqli_error($link));
  }

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $current_page = pager_default_initialize($count_rows, DJANGO_OBJECTS_PER_PAGE);

  $query = 'SELECT "" as `ingested?`, ci.do_id, ci.name, cit.name `type`, ' . //XX po.username, cc.name `collection`, ' .
           '  (SELECT GROUP_CONCAT(description) FROM transaction ct JOIN item_current_status cics ON (cics.last_transaction_id = ct.id) WHERE cics.item_id = ci.id ORDER BY ct.timestamp DESC) `last status` ' .
           'FROM `item` ci ' .
//XX           'JOIN auth_user po ON (po.id = ci.property_owner_id) ' .
           'JOIN item_type cit ON (cit.id = ci.type_id) ' .
           'JOIN collection cc ON (cc.id = ci.primary_collection_id) ' .
           $where_clause .
           'LIMIT ' . (($page) ? $page * DJANGO_OBJECTS_PER_PAGE . ', ' : '') . DJANGO_OBJECTS_PER_PAGE;

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT); // mysql_query($query, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
  }

  $count_rows_markup = number_format($count_rows) . " workflow objects " . (($page <> 0) ? " (page " . $page . " of " . round($count_rows / DJANGO_OBJECTS_PER_PAGE) . ")" : "");
  $headings = $rows = array();

  while ($row = mysqli_fetch_assoc($result)) {
    if (count($headings) < 1) {
      $headings = array_keys($row);
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
    $rows[] = $row;
  }

  mysqli_free_result($result);
  mysqli_close($link);
  $pager = theme('pager', array('quantity',$count_rows));
  $table_markup = theme('upitt_workflow_report',
      array('table' => $pager . theme('table', array('rows'=>$rows,'header'=>$headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'count_rows' => $count_rows_markup,
            'links' => array(),
            'post_to' => '/workflow',
            'collection_filter_choices' => upitt_workflow_get_collection_choices(),
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);
  return $form;
}

function upitt_workflow_objectreport_form($do_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $detail_query = 'SELECT b.batch_external_id `name`, b.batch_description `title`, b.mapto_collections, b.batch_id `batch_id` ' .
                  'FROM `item` i ' .
                  'JOIN batch_item bi ON (bi.item_id = i.id) ' .
                  'JOIN batch b ON (bi.batch_id = b.batch_id) ' .
                  'WHERE i.do_id = \'' . $do_id . '\'';

  $result = mysqli_query($link, $detail_query); // mmysql_query($detail_query, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysql_error() . "\n";
    $message .= 'Whole query: ' . $detail_query;
    die($message);
  }

  $title = "Workflow object | " . $do_id;
  $files_headings = $status_headings = $files_rows = $status_rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['batch_id']) && isset($row['name'])) {
      $row['batch'] = l($row['name'], '/workflow/batch/' . $row['batch_id']) . '<br />';
      unset($row['batch_id']);
    }
    else {
      $row['batch'] = 'n/a';
    }
    $details = theme('workflow_item_details', array('details' => $row));
  }
  if (!$details) {
    drupal_set_message('Workflow object "' . $do_id . '" not found.  <a href="javascript:history.go(-1);">Return to previous page.</a>');
    drupal_goto(variable_get('site_404', ''));
  }
  $links = array(l('All Django objects', '/workflow'));

  // if there is any selection of the filters, then the data must be filtered with a WHERE clause, else don't add that condition
  $filters_use = $filters_mimetype = '';

  if (isset($_GET['use_filter']) && $_GET['use_filter'] <> '') {
    $filters_use = upitt_workflow_filters_clause($_GET['use_filter']);
  }
  if (isset($_GET['mimetype_filter']) && $_GET['mimetype_filter'] <> '') {
    $filters_mimetype = upitt_workflow_filters_clause($_GET['mimetype_filter']);
  }

  // allow to filter on cif.mime_type and cif.use
  // Files table --
  $files_query = 'SELECT f.name `name`, f.path `path`, f.mime_type `mime_type`, f.use `use`, f.timestamp ' .
                 'FROM item_file f ' .
                 'LEFT JOIN item i ON (f.item_id=i.id) ' .
                 'WHERE i.do_id=\'' . $do_id . '\'' .
                 (($filters_use) ? ' AND i.use IN ' . $filters_use : '') .
                 (($filters_mimetype) ? ' AND i.mime_type IN ' . $filters_mimetype : '') .
                 ' ' .
                 'ORDER BY f.name';

  $result = mysqli_query($link, $files_query); // mmysql_query($files_query, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysql_error() . "\n";
    $message .= 'Whole query: ' . $files_query;
    die($message);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($files_headings) < 1) {
      $files_headings = array_keys($row);
    }
    if (array_key_exists('do_id', $row)) {
      $pid = 'pitt:' . $row['do_id'];
      if ($obj = islandora_object_load($pid)) {
        $row['do_id'] = l($row['do_id'], '/islandora/object/' . $pid, array('attributes' => array('target' => '_blank')));
      } else {
        $row['do_id'] = l($row['do_id'], '/workflow/object/' . $row['do_id']);
      }
    }
    $files_rows[] = $row;
  }

  // simple status ~ transactions table
  $status_query = 'SELECT t.description, t.`timestamp` `time` '.
//XX  $status_query = 'SELECT po.username `user`, ct.description, ct.`timestamp` '.
                  'FROM item i ' .
                  'JOIN transaction t ON (t.item_id = i.id) ' .
//XX                  'JOIN auth_user po ON (po.id = i.property_owner_id) ' .
                  'JOIN collection co ON (co.id = i.primary_collection_id) ' .
                  'WHERE i.do_id=\'' . $do_id . '\' ' .
                  'ORDER BY i.do_id DESC, t.`timestamp` ASC';
dpm($status_query);
  $result = mysqli_query($link, $status_query); // mmysql_query($status_query, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysql_error() . "\n";
    $message .= 'Whole query: ' . $status_query;
    die($message);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($status_headings) < 1) {
      $status_headings = array_keys($row);
      $status_headings[] = 'when';
    }
    $dbDate = new DateTime($row['time']);
    $currDate = new DateTime(date('Ymd H:i:s'));
    $interval = $currDate->diff($dbDate);
    $ago_arr = array();
    if ($interval->y > 0) {
      $ago_arr[] = $interval->y . " yr" . ($interval->y == 1 ? '' : 's');
    }
    if ($interval->m > 0) {
      $ago_arr[] = $interval->m . " mo";
    }
    if ($interval->d > 0) {
      $ago_arr[] = $interval->d . " d";
    }
    if ($interval->h > 0) {
      $ago_arr[] = $interval->h . " h";
    }
    if ($interval->i > 0) {
      $ago_arr[] = $interval->i . " m";
    }
    if ($interval->s > 0) {
      $ago_arr[] = $interval->s . " s";
    }

    $row['when'] = implode(", ", $ago_arr) . ((count($ago_arr) > 0) ? ' ago' : '');

    $status_rows[] = $row;
  }
  mysqli_close($link);

  $table_markup = theme('upitt_workflow_report_with_detail_table',
      array('details' => $details,
            'status_table' => theme('table', array('rows'=>$status_rows,'header'=>$status_headings)),
            'files_table' => theme('table', array('rows'=>$files_rows,'header'=>$files_headings)),
            'title' => $title,
            'links' => $links,
            'post_to' => '/workflow/object/' . $do_id,
           )
    );

  $content = array(
    'table_markup' => array('#markup' => $table_markup),
  );
  return $content;
}


