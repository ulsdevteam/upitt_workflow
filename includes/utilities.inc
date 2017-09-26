<?php

/** 
 * Simple lookup function to provide mapping between the legacy workflow "type_of_resource" to the Islandora fedora model name.
 */
function upitt_workflow_get_workflow_to_fedora_object_mappings() {
  return array('' => ' - select model - ',
               'sp-audioCModel' => 'Audio',
               'bookCModel' => 'Book',
               'compoundCModel' => 'Compound Object',
               'findingAidCModel' => 'Finding Aids (pending development)',
               'sp_large_image_cmodel' => 'Large Images',
               'manuscriptCModel' => 'Manuscript',
               'newspaperIssueCModel' => 'Newspaper',
               'oralhistoriesCModel' => 'Oral Histories',
               'sp_pdf' => 'PDF object',
               'sp_videoCModel' => 'Video',
              );
}

/**
 * This will return the fedora model name (without the "info:fedora/islandora:" prefix)
 * that matches the item_type.name values.
 *
 * @param string $type
 * @return string
 */
function upitt_workflow_wf_type_to_fedora_model($type) {
  $mapping = array(
      'manuscript' => 'manuscriptCModel',
      'image' => 'sp_large_image_cmodel',
      'text - cataloged' => 'bookCModel',
      'text - uncataloged' => 'bookCModel',
      'map' => 'sp_large_image_cmodel',
      'pdf' => 'sp_pdf',
      'georeferenced map' => 'sp_large_image_cmodel',
      'newspaper - cataloged' => 'newspaperCModel',
      'compound object' => 'compoundCModel',
      'audio' => 'sp-audioCModel',
      'video' => 'sp_videoCModel',
      'oral histories' => 'oralhistoriesCModel',
      );
  return isset($mapping[$type]) ? $mapping[$type] : '';
}

/**
 * Helper function to create the search form filter choices for collections in workflow_django
 */
function upitt_workflow_get_collection_choices() {
  $rows = array('' => 'Select any collection');
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $query = 'SELECT `name` FROM collection ORDER BY `name`';

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[$row['name']] = $row['name'];
  }
  mysqli_close($link);

  return $rows;
}

function upitt_workflow_get_mysql_options($table_name, $option_name = '', $key_field, $value_field, $sort_field = '', $option_value = 0) {
  $query = 'SELECT ' . $key_field . ', ' . $value_field . ' FROM ' . $table_name . ($sort_field ? ' ORDER BY ' . $sort_field : '');
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }

  $rows = ($option_name) ? array($option_value => 'Select ' . $option_name) : array();
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[$row[$key_field]] = $row[$value_field];
  }
  mysqli_close($link);
  return $rows;
}

function upitt_workflow_get_solr_options($solr_query, $key_field, $value_field, $sort_field = '') {
  $query_processor = new IslandoraSolrQueryProcessor();
  $query_processor->solrQuery = $solr_query;
  if ($sort_field) {
    $query_processor->sort = $sort_field;
  }
  $query_processor->solrStart = 0;
  $query_processor->solrLimit = 40000;
  $query_processor->solrParams = array('fl' => $key_field . (($value_field <> $key_field) ? ',' . $value_field : ''));

  $url = parse_url(variable_get('islandora_solr_url', 'localhost:8080/solr'));
  $solr = new Apache_Solr_Service($url['host'], $url['port'], $url['path'] . '/');
  $solr->setCreateDocuments(FALSE);
  $results = array();
  try {
    $search_results = $solr->search($query_processor->solrQuery, $query_processor->solrStart, $query_processor->solrLimit, $query_processor->solrParams, 'GET');
    $tmp = json_decode($search_results->getRawResponse(), TRUE);
    if ($tmp['response']['numFound'] > 0) {
      foreach ($tmp['response']['docs'] as $rec) {
        $results[$rec[$key_field]] = $rec[$value_field];
      }
    }
  }
  catch (Exception $e) {
  }
  return $results;
}

/**
 * Helper function to look up the names of the collections based on a possible comma-separated list
 */
function upitt_workflow_lookup_collection_names() {
  $rows = array();
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $query = 'SELECT * FROM collection ORDER BY `name`';

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[$row['id']] = $row;
  }
  return $rows;
}

function upitt_workflow_collection_id_map_names($ids = '', $collections = array()) {
  if ($ids) {
    $retvals = array();
    $ids_arr = explode(",", $ids);
    foreach ($ids_arr as $id) {
      $id = trim($id);
      if (isset($collections[$id])) {
        $retvals[] = $collections[$id]['c_id'];
      }
      else {
        $found = FALSE;
        foreach ($collections as $collection) { 
          if (!$found && $collection['PID'] == $id) {
            $retvals[] = $collection['c_id'];
            $found = TRUE;
          }
        }
      }
    }
    return implode(", ", $retvals);
  }
}

/**
 * Function needed by the edit batch & create batch forms to sync the solr collection choices with those that are in the collection table.
 *
 * @param array $pids
 *   The collection PID values.  These are structured like array('PID1' => 'PID1', 'PID2' => 'PID2')
 * @param array $collection_options
 *   The array of collectoins PID / fgs_label_s values from Solr.  These are structured like
 *   array('PID1' => 'fgs_label1', 'PID2' => 'fgs_label_s_2');
 */
function upitt_workflow_sync_solr_collections($pids, $collection_options) {
  // step 1, identify which ones need to be inserted.
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $query = "SELECT `PID`, `name` FROM collection WHERE `PID` IN ('" . implode("','", $pids) . "')";
  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }
  $found_pids = $bad_pids = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $found_pids[$row['PID']] = $row['name'];
  }
  foreach ($pids as $pid) {
    if (array_search($pid, $found_pids) === FALSE) {
      // not found, will need to update a record in mysql for this.
      $bad_pids[$pid] = $collection_options[$pid];
    }
  }
  foreach ($bad_pids as $pid => $name) {
    $sql = "REPLACE INTO `collection` (`c_id`, `PID`, `name`) VALUES ('". $pid . "', '". $pid . "', '" . upitt_workflow_safe_qstring($link, $name) . "')";
    $result = mysqli_query($link, $sql);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $sql);
    }
  }
  mysqli_close($link);
}

function upitt_workflow_get_allowed_listnames() {
  return array('action', 'collection', 'content_types', 'item_type', 'property_owner', 'transaction_actions', 'wflocal_fedora_site', 'workflow_sequence', 'workflow_sequence_actions');
}

function upitt_workflow_update_object_status($object_id, $action) {
  return TRUE;
}

function upitt_workflow_safe_qstring($link, $in) {
  return mysqli_real_escape_string($link, urldecode(trim($in)));
}

/**
 * This will query the islandora_workflow database for the status records related to a given object_id (item.do_id)
 */
function upitt_workflow_get_status($object_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $query = 'SELECT t.transaction_action_id, ta.description, t.`timestamp` `time` ' .
           'FROM item i ' .
           'JOIN `transaction` t ON (t.item_id = i.id) ' .
           'JOIN `transaction_actions` ta ON (ta.id = t.transaction_action_id) ' .
           'WHERE i.do_id="' . upitt_workflow_safe_qstring($link, $object_id) . '" ' .
           'ORDER BY t.`timestamp` ASC';

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }

  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[] = $row;
  }
  mysqli_close($link);
  return $rows;
}

/**
 * This will inspect the status values for the given object and return only the
 * links relevant to drive the workflow along for this object.
 *
 * @param string $do_id
 * @param array $status with 'description' and 'timestamp' values for each element
 * @return array of #markup
 */
function upitt_workflow_drive_along_links($do_id, $status) {
  $item = menu_get_item();
  $active_attrib = array('attributes' => array('class' => 'active'));

  $ingest_prepared = $ingested = $published_on_site = $files_moved = FALSE;
  foreach ($status as $a_status) {
    $ingest_prepared |= ($a_status['transaction_action_id'] == UPITT_WORKFLOW_ACTION_ISLANDORA_INGEST_PREPARED);
    $ingested |= ($a_status['transaction_action_id'] == UPITT_WORKFLOW_ACTION_INGESTED_COMPLETELY);
    $published_on_site |= ($a_status['transaction_action_id'] == UPITT_WORKFLOW_ACTION_OBJECT_PUBLISHED_ON_SITE);
    $files_moved |= ($a_status['transaction_action_id'] == UPITT_WORKFLOW_ACTION_INCOMING_INGEST_FILES_MOVED);
  }
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  // Get the batch record related to this object - to inspect for other batch-level action links
  $batch_id = upitt_workflow_get_batch_id($do_id, FALSE);
  $batch = upitt_workflow_batch_load($batch_id);
  // TODO

  $pid = $ingest_namespace . $do_id;
  $headings = array('Action', 'Operation');
  $links = array();

  $links[] = (($files_moved) ? 'Batch files <b class="good">have been moved</b>' : 'Batch files <b class="bad">have not been moved</b>');
  $links[] = (($ingest_prepared) ? 'Batch is <b class="good">prepared</b>' : l('Islandora Ingest Preprocess', '/workflow/batch/' . $batch_id . '/islandora_ingest',
      ($item['path'] == 'workflow/batch/%/islandora_ingest') ? $active_attrib : array()));

  $links[] = (($ingested) ? 'Batch is <b class="good">ingested</b>' :
      (($ingest_prepared && $files_moved) ? l('Islandora Ingest Preprocess (batch)', '/workflow/batch/' . $batch_id . '/islandora_ingest',
      ($item['path'] == 'workflow/batch/%/islandora_ingest') ? $active_attrib : array()) : ' - '));

  if (!$published_on_site) {
    // add a row to allow review of the batch in islandora
    if ($ingested) {
      $links[] = l('Review object', '/islandora/object/' . $pid,
        ($item['path'] == 'islandora/object/%') ? $active_attrib : array());
      $links[] = l('Relate to Site(s) / collection(s)', '/workflow/object/' . $do_id . '/do_publish',
        ($item['path'] == 'workflow/object/%/do_publish') ? $active_attrib : array());
    } elseif ($ingest_prepared) {
      $links[] = l('Ingest Now', '/workflow/object/' . $do_id . '/islandora_ingest_now',
        ($item['path'] == 'workflow/object/%/islandora_ingest_now') ? $active_attrib : array());
    }
  }
  else {
    $links[] = 'Object is <b class="good">published on site</b>';
  }
  return $links;
}

/**
 * Given an object identifier, this will return the batch that the item belongs to.  This will return NULL if the batch is not found.
 */
function upitt_workflow_batch_of_item($object_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $query = 'SELECT b.* ' .
           'FROM batch b ' .
           'JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
           'JOIN item i ON (i.id = bi.item_id) ' .
           'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '" LIMIT 1';

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }

  $row = mysqli_fetch_assoc($result);

  mysqli_close($link);
  return $row;
}

/**
 * This returns whether or not the batch_external_id value is also stored in the
 * items table as a do_id value.
 *
 * @param type $batch_external_id
 * @return type
 */
function upitt_workflow_batch_external_is_item($batch_external_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $query = 'SELECT i.id ' .
           'FROM item i ' .
           'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $batch_external_id) . '" LIMIT 1';

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }

  $row = mysqli_fetch_assoc($result);

  mysqli_close($link);
  return isset($row['id']);
}

/**
 * Will return just item file record for the given object_id (legacy :do_id value).
 */
function upitt_workflow_get_item($object_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $query = 'SELECT i.id, i.do_id, i.name, i.type_id, i.property_owner_id, i.primary_collection_id, ' .
           ' (SELECT b.mapto_collections FROM batch b JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
           '  WHERE bi.item_id = i.id ORDER BY bi.id DESC LIMIT 1) `collections`, ' .
           ' (SELECT b.mapto_site_id_values FROM batch b JOIN batch_item bi ON (bi.batch_id = b.batch_id) ' .
           '  WHERE bi.item_id = i.id ORDER BY bi.id DESC LIMIT 1) `sites` ' .
           'FROM item i ' .
           'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '"';

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }

  $retval = array();
  if ($row = mysqli_fetch_assoc($result)) {
    $retval = $row;
  }
  mysqli_close($link);
  return $retval;
}

/**
 * Will return all of the item file records associated with a specific object identifier.
 */
function upitt_workflow_get_item_files($object_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $query = 'SELECT itf.* ' .
           'FROM item i ' .
           'JOIN item_file itf ON (itf.item_id = i.id) ' .
           'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '" ' .
           'ORDER BY itf.id';

  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }

  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[] = $row;
  }
  mysqli_close($link);
  return $rows;
}


/**
 * This function will return the Solr records for members of the provided object PID;
 */
function upitt_workflow_get_object_members_solr_record($pid, $fl = '') {
  module_load_include('inc', 'islandora_solr', 'includes/utilities');
  $query_processor = new IslandoraSolrQueryProcessor();
  $members = array();
  // run two queries --
  // 1) to get the datastreams available on this object
  $query_processor->solrQuery = format_string('!field:!value', array(
    '!field' => 'RELS_EXT_isMemberOf_uri_ms',
    '!value' => '*' . str_replace(array("/", ":", "-", "+"), array("\/", "\:", "\-", "\+"), $pid),
   ));
  $params = array('sort' => 'PID ASC');
  if ($fl) {
    $params['fl'] = $fl;
  }
  $query_processor->solrStart = 0;
  $query_processor->solrLimit = 9999;
  $query_processor->solrParams = $params;

  $url = parse_url(variable_get('islandora_solr_url', 'localhost:8080/solr'));
  $solr = new Apache_Solr_Service($url['host'], $url['port'], $url['path'] . '/');
  $solr->setCreateDocuments(FALSE);
  try {
    $search_results = $solr->search($query_processor->solrQuery, $query_processor->solrStart, $query_processor->solrLimit, $query_processor->solrParams, 'GET');
    $tmp = json_decode($search_results->getRawResponse(), TRUE);

    $results = array();
    $numFound = $tmp['response']['numFound'];
    if ($tmp['response']['numFound'] > 0) {
      foreach ($tmp['response']['docs'] as $k=>$rec) {
        $members[] = $rec;
      }
    }
  }
  catch (Exception $e) {
    error_log('EXCEPTION in _save_solr_search_session : called from ' . $_SERVER['SERVER_NAME'] .
' - ' . $_SERVER['REQUEST_URI'] . '
' . print_r($e, true));
  }
  return $members;
}

/**
 * This function will return the Solr record for this object so that it can be compared to the expected values from the workflow record;
 */
function upitt_workflow_get_solr_record($pid) {
  module_load_include('inc', 'islandora_solr', 'includes/utilities');
  $query_processor = new IslandoraSolrQueryProcessor();

  // run two queries --
  // 1) to get the datastreams available on this object
  $query_processor->solrQuery = format_string('!field:!value', array(
    '!field' => 'PID',
    '!value' => str_replace(array("/", ":", "-", "+"), array("\/", "\:", "\-", "\+"), $pid),
   ));
  $query_processor->solrStart = 0;
  $query_processor->solrLimit = 1;
  $query_processor->solrParams = array();

  $url = parse_url(variable_get('islandora_solr_url', 'localhost:8080/solr'));
  $solr = new Apache_Solr_Service($url['host'], $url['port'], $url['path'] . '/');
  $solr->setCreateDocuments(FALSE);
  try {
    $search_results = $solr->search($query_processor->solrQuery, $query_processor->solrStart, $query_processor->solrLimit, $query_processor->solrParams, 'GET');
    $tmp = json_decode($search_results->getRawResponse(), TRUE);

    $results = array();
    $numFound = $tmp['response']['numFound'];
    if ($tmp['response']['numFound'] > 0) {
      foreach ($tmp['response']['docs'] as $k=>$rec) {
        $retval = $rec;
      }
    }
  }
  catch (Exception $e) {
    error_log('EXCEPTION in _save_solr_search_session : called from ' . $_SERVER['SERVER_NAME'] .
' - ' . $_SERVER['REQUEST_URI'] . '
' . print_r($e, true));
  }
  return $retval;
}

function upitt_workflow_is_paged_object($islandora_object) {
  return (is_object($islandora_object)) ? (!(array_search('islandora:bookCModel', $islandora_object->models) === FALSE) || 
    !(array_search('islandora:newspaperIssueCModel', $islandora_object->models) === FALSE) || 
    !(array_search('islandora:manuscriptCModel', $islandora_object->models) === FALSE) ) : FALSE;
}

/**
 * This will return a human readable version of "how long ago" for a given timetamp.
 */
function upitt_workflow_timeago_from_timestamp($timestamp) {
  $dbDate = new DateTime($timestamp);
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
  return implode(", ", $ago_arr) . ((count($ago_arr) > 0) ? ' ago' : '');
}

function upitt_workflow_sql_error_die($link, $sql) {
  $message  = 'Invalid query: ' . mysqli_error($link) . "<br>";
  $message .= 'Query: <pre>' . $sql ."</pre>";
  $bt = debug_backtrace();
  $variables = array('@sql' => $sql, '@backtrace' => $bt);
  watchdog('upitt_workflow', 'There was an error executing a database query.  The sql = @sql.  Debug backtrace @backtrace', $variables, WATCHDOG_ERROR);
  die($message);
}

/**
 * This function will return the parent collection PID values for the given object by looking at the object's batch 
 * record and the individual item mappings (stored in wflocal_local_item_fedora_collections)
 */
function upitt_workflow_get_object_fedora_collections($object_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  // Get item's collection mappings
  $item_query = 'SELECT fc.pid `pid` ' .
                'FROM item i ' .
                'JOIN wflocal_local_item_fedora_collections wifc ON (wifc.local_item_id = i.id) ' .
                'JOIN wflocal_fedora_collection fc ON (fc.id = wifc.fedora_collection_id) ' .
                'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '"';

  $result = mysqli_query($link, $item_query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $item_query);
  }
  // if this has a value, it is already in CSV format
  $row = mysqli_fetch_assoc($result);

  // Only look at the batch if there was no value set at the item level.
  if (isset($row['pid'])) {
    $retval = $row['pid'];
  }
  else {
    // Get item's batch collection mappings
/*    $i_batch_query = 'SELECT c.PID `pid` ' .
                     'FROM collection c ' .
                     'WHERE c.id IN ' .
                     ' (SELECT b.mapto_collections ' .
                     ' FROM item i ' .
                     ' JOIN batch_item bi ON (bi.item_id = i.id) ' .
                     ' JOIN batch b ON (b.batch_id = bi.batch_id) ' .
                     ' WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '")';
*/
    $i_batch_query = 'SELECT b.mapto_collections ' .
                     'FROM item i ' .
                     'JOIN batch_item bi ON (bi.item_id = i.id) ' .
                     'JOIN batch b ON (b.batch_id = bi.batch_id) ' .
                     'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '"';

    $result = mysqli_query($link, $i_batch_query);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $i_batch_query);
    }

    $rows = array();
    while ($row = mysqli_fetch_assoc($result)) {
//      $rows[$row['pid']] = $row['mapto_collections'];
      $rows[$row['mapto_collections']] = $row['mapto_collections'];
    }
    $retval = implode(',', $rows);
  }

  mysqli_close($link);
  return $retval;
}

/**
 * This function will return the site PID values for the given object by looking at the object's batch
 * record and the individual item mappings (stored in wflocal_local_item_fedora_sites)
 */
function upitt_workflow_get_object_fedora_sites($object_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  // Get item's site mappings
  $item_query = 'SELECT wfs.pid ' .
                'FROM item i ' .
                'JOIN wflocal_local_item_fedora_sites wifs ON (wifs.local_item_id = i.id) ' .
                'JOIN wflocal_fedora_site wfs ON (wfs.id = wifs.fedora_site_id) ' .
                'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '"';

  $result = mysqli_query($link, $item_query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $item_query);
  }
  // if this has a value, it is already in CSV format
  $row = mysqli_fetch_assoc($result);

  // Only look at the batch if there was no value set at the item level.
  if (isset($row['pid'])) {
    $retval = $row['pid'];
  }
  else {
    // Get item's batch site mappings
/*    $i_batch_query = 'SELECT wfs.pid `pid` ' .
                     'FROM wflocal_fedora_site wfs ' .
                     'WHERE wfs.id IN ' .
                     ' (SELECT b.mapto_site_id_values ' .
                     ' FROM item i ' .
                     ' JOIN batch_item bi ON (bi.item_id = i.id) ' .
                     ' JOIN batch b ON (b.batch_id = bi.batch_id) ' .
                     ' WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '")';
*/
    $i_batch_query = 'SELECT b.mapto_site_id_values ' .
                     'FROM item i ' .
                     'JOIN batch_item bi ON (bi.item_id = i.id) ' .
                     'JOIN batch b ON (b.batch_id = bi.batch_id) ' .
                     'WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $object_id) . '"';

    $result = mysqli_query($link, $i_batch_query);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $i_batch_query);
    }

    $rows = array();
    while ($row = mysqli_fetch_assoc($result)) {
      $rows[$row['mapto_site_id_values']] = $row['mapto_site_id_values'];
    }
    $retval = implode(',', $rows);
  }

  mysqli_close($link);
  return $retval;
}

/**
 * Used by sp_pdf and sp_videoCModel processing to work with the subfolders as objects.  Given
 * the folder, this will return all the files and folders in an associative array that sets each
 * value as the relative file path (from the $incoming_folder).
 *
 * @param string $incoming_folder
 * @return array
 */
function upitt_workflow_generic_get_directory($incoming_folder) {
  $folders = upitt_workflow_file_scan_directory($incoming_folder, '', TRUE, TRUE);
  $tmp_files = array();
  foreach ($folders as $foldername => $contents) {
    $folder = trim($foldername);
    $subfolder = $incoming_folder . '/' . $folder;
    $files = upitt_workflow_file_scan_directory($subfolder, '', TRUE, FALSE);
    foreach ($files as $key => $file) {
      if (is_array($file)) {
        $file = $file[0];
      }

      $tmp_files[$file] = $folder . '/' . $file;
    }
  }
  return $tmp_files;
}

/**
 * Helper function to return a set of filenames from a given path -- with the option to filter ($mask)
 * and an option to recursively ($recurse) search for files under the initial directory path ($dir).
 */
function upitt_workflow_file_scan_directory($dir, $mask = '', $recurse = FALSE, $returnOnlyFolders = FALSE) {
  $result = array();
  if (!is_dir($dir)) {
    return $result;
  }
  $cdir = scandir($dir);
  foreach ($cdir as $key => $value) {
    if (!in_array($value,array(".",".."))) {
      if ($recurse && (is_dir($dir . DIRECTORY_SEPARATOR . $value))) {
        $result[$value] = upitt_workflow_file_scan_directory($dir . DIRECTORY_SEPARATOR . $value, $mask, $recurse, $returnOnlyFolders);
      }
      elseif (!$recurse || !$returnOnlyFolders) {
        if (!$mask || (strstr($value, $mask))) {
          $result[] = $value;
        }
      }
    }
  }
  return $result;
}

/**
 * This will run XML to MODS transformation and save resultant MODS
 * over the original file.
 *
 * This same effect could be achieved using a template system because xslt is
 * relatively slow.  For now, the xslt transform method is used.
 *
 * Returns the filename for the new MODS file.
 */
function upitt_workflow_XMLtoMODS($xml_filename) {
  $xsl = str_replace("/includes", "", dirname(__FILE__).'/transforms/sheet2mods.xsl'); // xml2mods.xsl');

  $xml_file = file_get_contents($xml_filename);
  $MODS = ($xml_file) ? upitt_workflow_runXslTransform(
            array(
              'xsl' => $xsl,
              'input' => $xml_file,
            )
          ) : '';

  // This file must be deleted in the process function that called this.
  $bytes_written = file_put_contents($xml_filename, $MODS);

  echo "<h3>after XSL transform</h3><blockquote><pre style='color:#28f'>" . htmlspecialchars(print_r($MODS, true)) . "</pre></blockquote>";

  return ($bytes_written) ? $xml_filename : '';
}

function upitt_workflow_MARCtoMODS($pid, $marcxml_filename) {
  $marc_file = file_get_contents($marcxml_filename);
  // Get the DC by transforming from MODS.
  $new_MODS = ($marc_file) ? upitt_workflow_runXslTransform(
            array(
              'xsl' => drupal_get_path('module', 'upitt_workflow') . '/transforms/MARC21slim2MODS3-5.xsl',
              'input' => $marc_file,
            )
          ) : '';
  if ($new_MODS) {
    $filename = tempnam(file_directory_temp(), str_replace(":", "_", $pid) . "_MODS_xml_derived");
    // This file must be deleted in the process function that called this.
    file_put_contents($filename, $new_MODS);
    return $filename;
  }
  else {
    return '';
  }
}

function upitt_workflow_MODStoDC($pid, $modsxml_filename) {
  $mods_file = file_get_contents($modsxml_filename);
  // Get the DC by transforming from MODS.
  $new_DC = ($mods_file) ? upitt_workflow_runXslTransform(
            array(
              'xsl' => drupal_get_path('module', 'islandora_batch') . '/transforms/mods_to_dc.xsl',
              'input' => $mods_file,
            )
          ) : '';
  if ($new_DC) {
    $filename = tempnam(file_directory_temp(), str_replace(":", "_", $pid) . "_DC_xml_derived");
    // This file must be deleted in the process function that called this.
    file_put_contents($filename, $new_DC);
    return $filename;
  }
  else {
    return '';
  }
}

function upitt_workflow_runXslTransform($info) {
  $xsl = new DOMDocument();
  $xsl->load($info['xsl']);
  $input = new DOMDocument();
  $input->loadXML($info['input']);

  $processor = new XSLTProcessor();
  $processor->importStylesheet($xsl);

/*
  if (isset($info['php_functions'])) {
    $processor->registerPHPFunctions($info['php_functions']);
  }
*/

  // XXX: Suppressing warnings regarding unregistered prefixes.
  return $processor->transformToXML($input);
}

/**
 * Will handle the CSV values for dates and create a "display date" and a "sort date" value as best as it can assuming:
 *      CSV value		Display date				Sort date
 * -------------------------------------------------------------------------------------------------
 *	1945/1955		1945-1955				1945-01-01T00:00:00-05:00
 * a	1945/1955 		ca. 1945-1955				1945-01-01T00:00:00-05:00
 *	1945-03/1955-04 	March 1945- April 1955			1945-03-01T00:00:00-05:00
 * a	1945-03/1955-04 	ca. March 1945- April 1955		1945-03-01T00:00:00-05:00
 * 	1945-03-05/1955-04-23	March 5, 1945- April 23, 1955		1945-03-05T00:00:00-05:00
 * a	1945-03-05/1955-04-23 	ca. March 5, 1945- April 23, 1955	1945-03-05T00:00:00-05:00
 */
function upitt_workflow_make_custom_csv_dates($short_date, $suppress_display_messages = FALSE) {
  $short_date = trim($short_date);
  $short_date_length = strlen($short_date);
  $h_format = 'F j, Y';
  $from_date = '';
  // Handle the special cases where there is a date range provided
  if ($short_date_length == 9 && ($short_date[4] == '/' || $short_date[4] == '-')) {
    $from_date = substr($short_date, 0, 4) . '-01-01';
    $to_date = substr($short_date, 5, PHP_INT_MAX) . '-01-01';
    $h_format = 'Y';
  }
  elseif ($short_date_length == 15 && ($short_date[7] == '/' || $short_date[7] == '-')) {
    $from_date = substr($short_date, 0, 7);
    $to_date = substr($short_date, 8, PHP_INT_MAX);
    $h_format = 'F Y';
  }
  elseif ($short_date_length == 21 && ($short_date[10] == '/' || $short_date[10] == '-')) {
    $from_date = substr($short_date, 0, 10);
    $to_date = substr($short_date, 11, PHP_INT_MAX);
  }
  if ($from_date) {
    $timestamp = strtotime($from_date);
    $timestamp_to_date = strtotime($to_date);
    $display_date = date($h_format, $timestamp) . '-' . date($h_format, $timestamp_to_date);
    return array(date('c', $timestamp), $display_date);
  }
  if (strstr($short_date, '-') == '') {
    if ($short_date_length == 4) {
      $short_date .= '-01-01';
      $h_format = 'Y';
    }
    elseif ($short_date_length == 6) {
      $short_date = substr($short_date, 0, 4) . '-' . substr($short_date, 4, 2) . '-01';
      $h_format = 'F Y';
    }
    elseif ($short_date_length <> 8 && !$from_date) {
      if ($short_date <> '') {
        if (!$suppress_display_messages) {
          drupal_set_message('bad date format for date value of "' . $short_date . '".', 'error');
        }
        return '';
      }
      else {
        return '';
      }
    }
  }
  else {
    if ($short_date_length == 7) {
      $short_date .= '-01';
      $h_format = 'F Y';
    }
    elseif ($short_date_length <> 10 && !$from_date) {
      if (!$suppress_display_messages) {
        drupal_set_message('bad date format for date value of "' . $short_date . '".', 'error');
      }
      return '';
    }
  }
  $timestamp = strtotime($short_date);
  return array(date('c', $timestamp), date($h_format, $timestamp));
}

/**
 * Helper function for creating MODS from CSV - this will inspect the active row for the value in the 
 * 'normalized_date_qualifier' field to see whether or not the date field value supplied row is approximate.
 */
function upitt_workflow_fix_if_csv_date_approximate($headers, &$row) {
  $normalized_date_qualifier_index = array_search('normalized_date_qualifier', $headers);
  $date_index = array_search('date', $headers);
  // echo "<h3>normalized_date_qualifier_index = " . $normalized_date_qualifier_index . ", date_index = " . $date_index ."</h3>";
  if ($normalized_date_qualifier_index && isset($row[$normalized_date_qualifier_index])) {
    if ($row[$normalized_date_qualifier_index] == 'yes' || $row[$normalized_date_qualifier_index] == 'approximate') {
      $row[$date_index] = 'ca. ' . $row[$date_index];
    }
  }
  // echo "<pre style='color:red'>" . print_r($row, true)."</pre>";
}

/**
 * This still provides a test as to whether or not a field gets into the initial XML from the sheet - and the headings title
 * that would match for that field.
 */
function upitt_workflow_get_csv_header_xpath_mappings() {
  return array(
    'genre'                     => '/mods:mods/mods:genre',
    'abstract'                  => '/mods:mods/mods:abstract',
    'description'               => '/mods:mods/mods:abstract',
    'type_of_resource'          => '/mods:mods/mods:typeOfResource',
    'gift_of'                   => '/mods:mods/mods:note[@type="donor"]',
    'address'                   => '/mods:mods/mods:note[@type="address"]',
    'subject'                   => '/mods:mods/mods:subject/mods:topic',
    'contributor'               => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="contributor") and @type="text"]]',
    'creator'                   => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="creator") and @type="text"]]',
    'depositor'                 => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="depositor") and @type="text"]]',
    'identifier'                => '/mods:mods/mods:identifier[@type="pitt"]',
    'source_id'                 => '/mods:mods/mods:identifier[@type="source"]',
    'source_identifier'         => '/mods:mods/mods:identifier[@type="source"]',
    'title'                     => '/mods:mods/mods:titleInfo/mods:title',
    'publisher'                 => '/mods:mods/mods:originInfo/mods:publisher',
    'date_digitized'            => '/mods:mods/mods:originInfo/mods:dateCaptured',
    'format'                    => '/mods:mods/mods:physicalDescription/mods:form',
    'subject_name'              => '/mods:mods/mods:subject[@authority="lcsh"]/mods:name',
    'dimension'                 => '/mods:mods/mods:physicalDescription/mods:extent',
    'source_citation'           => '/mods:mods/mods:relatedItem/note[@type="prefercite"]',
    'subject_lcsh'              => '/mods:mods/mods:subject[@authority="lcsh"]/mods:topic',
    'source_collection_id'      => '/mods:mods/mods:relatedItem[@type="host"]/mods:identifier',
    'subject_local'             => '/mods:mods/mods:subject[@authority="local"]/mods:topic',
    'sort_date'                 => '/mods:mods/mods:originInfo/mods:dateOther[@type="sort"]',
    'subject_location'          => '/mods:mods/mods:subject[@authority="lcsh"]/mods:geographic',
    'date'                      => '/mods:mods/mods:originInfo/mods:dateOther[@type="display"]',
    'scale'                     => '/mods:mods/mods:subject/mods:cartographics/mods:scale',
    'source_collection'         => '/mods:mods/mods:relatedItem/mods:titleInfo/mods:title',
    'copyright_status'          => '/mods:mods/mods:accessCondition/copyrightMD:copyright/@copyright.status',
    'source_container'          => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="container"]',
    'source_ownership'          => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="ownership"]',
    'publication_status'        => '/mods:mods/mods:accessCondition/copyrightMD:copyright/@publication.status',
    'pub_place'                 => '/mods:mods/mods:originInfo/mods:place/mods:placeTerm[@type="text"]',
    'source_collection_date'    => '/mods:mods/mods:relatedItem[@type="host"]/mods:originInfo/mods:dateCreated',
    'normalized_date'           => '/mods:mods/mods:originInfo/mods:dateCreated[@encoding="iso8601" @keyDate="yes"]',
    'rights_holder'             => '/mods:mods/mods:accessCondition/copyrightMD:copyright/copyrightMD:rights.holder/copyrightMD:name',
    'normalized_date_qualifier' => '/mods:mods/mods:originInfo/mods:dateCreated[@qualifier="approximate" @encoding="iso8601" @keyDate="yes"]',
    // NEW fields for Oral Histories
    'interviewee'               => '',
    'interviewer'               => '',
    'parent_id'                 => '', // same as parent_identifier
    'parent_identifier'         => '', // same as parent_id
    // These field values are not represented in a MODS file, but will appear
    // in the CSV file.
    'batch' => '',
    'collection' => '',
    'file_name' => '',
    'filename' => '',
    'location' => '',
    // NEW fields that will appear in the CSV file, but not to be transformed to
    // a MODS node.
    'path_to_master_wav' => '',
  );
}

/**
 * Get the batch set id related to a queued object.
 *
 * @param string $pid
 *   The object id identifying the queued batch object.
 *
 * @return int
 *   The id of the batch set the object is apart of.
 */
function upitt_workflow_get_objects_from_set($sid) {
  $object_ids = db_select('islandora_batch_queue', 'q')
    ->fields('q', array('id'))
    ->condition('sid', $sid)
    ->execute()
    ->fetchAllAssoc('id', PDO::FETCH_ASSOC); 
  return $object_ids;
}

function upitt_workflow_get_batch_id($batch_external_id, $create_if_not_exists = FALSE) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $sql = 'SELECT `batch_id` FROM `batch` WHERE `batch_external_id` = "' . mysqli_real_escape_string($link, $batch_external_id) . '"';

  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $batch_id = 0;
  if ($row = mysqli_fetch_assoc($result)) {
    $batch_id = $row['batch_id'];
  }
  elseif ($create_if_not_exists) {
    // Create a new batch record that has the batch_external_id and return that record's batch_id
    $sql_insert = 'INSERT INTO `batch` (batch_external_id) VALUES (\'' . $batch_external_id . '\')';
    mysqli_query($link, $sql_insert);
    mysqli_close($link);
    // recurse, but be sure to not pass $create_if_not_exists = TRUE because that could lead to endless recursion.
    return upitt_workflow_get_batch_id($batch_external_id, FALSE);
  }

  mysqli_close($link);
  return $batch_id;
}

/**
 * Loads a batch record given the batch_external_id value.
 *
 * @param string $batch_external_id
 * @return array representing the batch record
 */
function upitt_workflow_batch_load_by_batch_external_id($batch_external_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $batch_detail = 'SELECT * ' .
                  'FROM batch ' .
                  'WHERE batch_external_id = "' . mysqli_real_escape_string($link, $batch_external_id) . '" LIMIT 1';

  $batch = array();

  $result = mysqli_query($link, $batch_detail);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $batch_detail);
  }

  $batch = $fields = array();
  if ($row = mysqli_fetch_assoc($result)) {
    $fields = array_keys($row);
    foreach ($fields as $field) {
      $batch[$field] = $row[$field];
    }
  }

  mysqli_close($link);
  return $batch;
}

/**
 * Loads a batch record given the batch_id value.
 *
 * @param type $batch_id
 * @return array representing the batch record
 */
function upitt_workflow_batch_load($batch_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $batch_detail = 'SELECT * ' .
                  'FROM batch ' .
                  'WHERE batch_id = \'' . $batch_id . '\' LIMIT 1';

  $batch = array();

  $result = mysqli_query($link, $batch_detail);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $batch_detail);
  }

  $batch = $fields = array();
  if ($row = mysqli_fetch_assoc($result)) {
    $fields = array_keys($row);
    foreach ($fields as $field) {
      $batch[$field] = $row[$field];
    }
  }

  mysqli_close($link);
  return $batch;
}

/**
 * Helper function that will return the list of datastreams that are currently in
 * use from running a Solr query.
 *
 * @return string
 */
function upitt_workflow_get_datastreams($pid = '') {
  $query_processor = new IslandoraSolrQueryProcessor();
  if ($pid) {
    $query_processor->solrQuery = 'PID:' . str_replace(array("/", ":", "-", "+"), array("\/", "\:", "\-", "\+"), $pid);
  } else {
    $query_processor->solrQuery = '*:*';
  }
  $query_processor->solrStart = 0;
  $query_processor->solrLimit = 0;
  $query_processor->solrParams = array('facet' => 'true',
                                       'facet.query' => 'fedora_datastreams_ms:["" TO *]',
                                       'facet.field' => 'fedora_datastreams_ms');

  $url = parse_url(variable_get('islandora_solr_url', 'localhost:8080/solr'));
  $solr = new Apache_Solr_Service($url['host'], $url['port'], $url['path'] . '/');
  $solr->setCreateDocuments(FALSE);
  $results = array();
  try {
    $search_results = $solr->search($query_processor->solrQuery, $query_processor->solrStart, $query_processor->solrLimit, $query_processor->solrParams, 'GET');
    $tmp = json_decode($search_results->getRawResponse(), TRUE);
    if ($tmp['response']['numFound'] > 0) {
      foreach ($tmp['facet_counts']['facet_fields']['fedora_datastreams_ms'] as $key => $num) {
        $results[$key] = $key . ' (' . $num . ')';
      }
    }
  }
  catch (Exception $e) {
  }
  return $results;
}

/**
 * Helper function that will load a CSV spreadsheet by name $csv_file, and return
 * it as an associative array.  This will make sure that the headers are all lowercase.
 * 
 * @param string $csv_file
 * @return array('header' => $file_header, 'rows' => $file_rows)
 * @throws Exception
 */
function upitt_workflow_csv_file_as_array($csv_file) {
  $max_csv_rows_for_display = 200;
  $file_rows = $file_header = array();
  if (file_exists($csv_file)) {
    $row = 0;
    if (($handle = fopen($csv_file, "r")) !== FALSE) {
      try {
        while (($data = fgetcsv($handle)) !== FALSE) {
          $row++;
	  if ($row < $max_csv_rows_for_display) {
            if ($row > 1) {
              $file_rows[] = $data;
            }
            else {
              $tmp_data = $data;
              foreach ($tmp_data as $i => $val) {
                $tmp_data[$i] = str_replace(" ", "_", trim(strtolower($val)));
              }
              $file_header = $tmp_data;
            }
          }
        }
        if ($row > $max_csv_rows_for_display) {
          // Subtract the header row.
          $data_rows = $row - 1;
          drupal_set_message('There are ' . number_format($data_rows) . ' rows in the CSV file "' . $csv_file . '".  Only the first ' . $max_csv_rows_for_display . ' rows are displayed.', 'warning');
        }
      }
      catch (Exception $e) {
        throw new Exception('The file "'.$csv_file.'" could not be parsed as a CSV.');
      }
      fclose($handle);
    }
  }
  return array('header' => $file_header, 'rows' => $file_rows);
}

/**
 * Helper function to return the value of the upitt_workflow_ingest_namespace - with the right trailing colon
 */
function upitt_workflow_get_ingest_namespace() {
  return rtrim(variable_get('upitt_workflow_ingest_namespace', 'pitt:'), ':') . ':';
}

/**
 * Helper function that will return the pid values from a spreadsheet.  This will
 * need to inspect the header column to find out which index represents the
 * identifier field - and then return that column of data from the rows.
 *
 * @param array $csv_arr  ('header' => $file_header, 'rows' => $file_rows)
 */
function upitt_workflow_csv_extract_identifiers($csv_arr) {
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $identifier_key = array_search('identifier', $csv_arr['header']);
  $ns_prefix = '';
  $ret_arr = array();
  if (!$identifier_key === FALSE) {
    foreach ($csv_arr['rows'] as $row) {
      $ret_arr[] = ((strstr($row[$identifier_key], $ingest_namespace) == '') ? $ingest_namespace : '') . $row[$identifier_key];
    }
  }
  return $ret_arr;
}

/**
 * This will return the ingest path for a given batch by combining the batch's host setting 
 * and the batch_external_id.  If the folder does not exist, it will be created.  It will be
 * named 'batch_{$batch_external_id}'.
 *
 * In the case when the batch is coming from the ftp to box.com, the system will use
 * the return the Default Ingest Path (configured admin/islandora/workflow) and the files
 * would the downloaded and extracted to this path from "box.com"
 *
 * Note about the folder permissions:
 *   The folder must be writable by apache as well as by other admin members of our department.
 *   A group has been created on archive-01 as well as pa-staff-01 named "webwrite" and apache
 *   is a member of that group as well as the users who would need write permissions there.
 * 
 * @param array $batch_values
 * @return string
 */
function upitt_workflow_batch_path($batch_values) {
  if (!is_array($batch_values) || !array_key_exists('batch_external_id', $batch_values)) {
    drupal_set_message('The batch path could not be determined.  Either the values are missing or ' .
            'there is no value for the batch "Name" (batch_external_id field) yet.', 'warning');
    return '';
  }
  $ingest_path = $batch_values['batch_host'];
  if (!$batch_values['batch_host'] || $ingest_path == 'ftp' || (ltrim($ingest_path, 'ftp:') <> $ingest_path)) {
    $ingest_path = variable_get('upitt_workflow_ingest_prepared_path', '/ingest/islandora_ingest');
  } elseif (strstr($ingest_path, ":")) {
    @list($ingest_host, $ingest_path) = explode(":", $ingest_path, 2);
    drupal_set_message('This batch is configured to use files from a different server.  That server needs ' .
            'to be configured as a mounted folder under the /ingest directory and the ' . l('Available Host Alias', '/admin/islandora/workflow/') .
            ' configuration would need to be updated to point to the mounted location.', 'error');
  } else {
    $ingest_path = $ingest_path;
  }
  $batch_path = rtrim($ingest_path, '/') . '/batch_' . $batch_values['batch_external_id'];
  if (!file_exists($batch_path)) {
    $call = mkdir($batch_path);
    if (!$call) {
      drupal_set_message('The batch folder "' . $batch_path . '" could not be created.', 'error');
    }
  }

  return (file_exists($batch_path)) ? $batch_path : '';
}

/**
 * This will return the export path for a given batch by batch_external_id and
 * optional datastream id.  If the folder does not exist, it will be created.
 * It will be named 'batch_{$dsid_}{$batch_external_id}'.
 *
 * @param string $batch_external_id
 * @return string
 */
function upitt_workflow_batch_export_path($batch_external_id, $dsid = '') {
  $export_path = variable_get('upitt_workflow_export_path', '/ingest/islandora_ingest');

  $batch_path = rtrim($export_path, '/') . '/batch_' . ($dsid ? $dsid . '_' : '') . $batch_external_id;
  if (!file_exists($batch_path)) {
    mkdir($batch_path);
  }
  return (file_exists($batch_path)) ? $batch_path : '';
}

/**
 * This function will fetch the config value for "Available Host Aliases for Batch ingest"
 * and split the alias_and_value value at the "=" character and then add items to the return 
 * associative array with the actual host value as the array keys and the alias is the value.
 * @return array
 */
function upitt_workflow_get_batch_host_values() {
  $arr = explode("\r\n", variable_get('upitt_workflow_hosts_for_ingest'), 2);
  $return_array = array(variable_get('upitt_workflow_ingest_prepared_path', '/ingest/islandora_ingest') => 'Default Incoming Ingest Path');

  foreach ($arr as $alias_and_value) {
    @list($alias, $value) = explode("=", $alias_and_value, 2);
    $alias = trim($alias);
    if ($alias && $value) {
      $return_array[$value] = $alias;
    }
  }
  return $return_array;
}

/**
 * This will prompt the user to create any paths that can not be found on the hosts that
 * are configured for the workflow module.
 *
 * Values for the batch_hosts_array will have an alias and then one of two kinds of possible values:
 *   "ftp" - currently designates the box.com as an incoming "host"
 * or
 *   servername:/folderpath/subfolder1/subfolder2
 * 
 * For now, the code will not try to create any folders within the box account.
 */
function upitt_workflow_create_host_paths($batch_hosts_array) {
  foreach ($batch_hosts_array as $alias_host_path) {

    @list($alias, $host_path) = explode("=", $alias_host_path, 2);
    if ($host_path <> 'ftp') {
      @list($host, $path) = explode(":", $host_path, 2);
      // if $host is the same as this current server's ID, then we can check the folder just using file_exists()
      if ($host == $_SERVER['SERVER_NAME']) {
        if (!file_exists($path)) {
          drupal_set_message(t('The path "' . $path . '" does not exist on this server "' . $host . '".  It will need to be created before the batch can be ingested'), 'warning');
        }
      }
      else {
        drupal_set_message(t('The system could not verify whether host "' . $host . '" has a folder named "' . $path . '".'), 'warning');
      }
    }
  }
  return;
}

/**
 * This will save or update the given batch record.  Special process will potentially need to populate the collection table if the choices are fedora collections.
 */
function upitt_workflow_save_batch($values, $collection_options) {
  global $user;
  $collections_values = $values['mapto_collections'];

  $batch_id = upitt_workflow_get_batch_id($values['batch_external_id'], TRUE);
  $existing_batch_record = ($batch_id) ? upitt_workflow_batch_load($batch_id) : array();

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  // these form_state values are not needed for the SQL and would cause problems, so removed them here.
  unset($values['submit']);  unset($values['form_build_id']); unset($values['form_token']); unset($values['form_id']); unset($values['op']);
  // this includes the TINYINT fields used for the checkboxes
  $batch_integer_fields = array('use_color_target', 'is_batch_request', 'is_batch_active', 'has_file', 'batch_property_owner_id',
      'batch_sequence_id', 'item_count', 'content_type_id');

  $batch_external_id = $values['batch_external_id'];

  if (!$batch_external_id) {
    drupal_set_message('The batch Name is a required field.  The batch could not be saved.', 'error');
    return;
  }
  $sql_fields = $sql_values = array();
  if ($batch_id) {
    $sql_fields[] = 'batch_id';
    $sql_values[] = $batch_id;
  }
  $valid_upload = NULL;
  $model = NULL;
  $batch_path = upitt_workflow_batch_path($values);
  // assume all fields need to be escaped and wrapped with '' characters -- EXCEPT for the integer fields.
  foreach ($values as $fieldname => $value) {
    // If the file is set, the actual file behind this must be moved to the incoming
    // ingest folder and the field value must be set to this path.
    if ($fieldname == 'file') {
      if (isset($_FILES['files']) && isset($_FILES["files"]["tmp_name"]['file'])) {
        $has_file = FALSE;
        $dest = $batch_path . '/metadata.csv';
        if ($_FILES['files']['error']['file'] == UPLOAD_ERR_OK) {
          $tmp_name = $_FILES["files"]["tmp_name"]['file'];
          if (move_uploaded_file($tmp_name, $dest)) {
            $valid_upload = upitt_workflow_validate_csv($dest, $batch_path);
            $value = $dest;
            $has_file = TRUE;
          }
          else {
            drupal_set_message(t('File move operation from "' . $tmp_name . '" to "' . $dest . '" failed'), 'error');
            return false;
            $value = NULL;
          }
        }
        if (($fieldname == 'file' && $value) || $fieldname <> 'file' || (!empty($existing_batch_record['file']))) {
          $sql_fields[] = $fieldname;
          if (!$value && isset($existing_batch_record['file']) && $fieldname == 'file') {
            $value = $existing_batch_record['file'];
            $has_file = TRUE;
            $valid_upload = upitt_workflow_validate_csv($existing_batch_record['file'], $batch_path);
          }
        }
      }
    } else {
      $sql_fields[] = $fieldname;
    }

    // Store this for final validation check on the CSV that depends on the batch's model.
    if ($fieldname == 'default_type_of_resource') {
      $model = $value;
    }

    if ($fieldname == 'mapto_collections') {
      upitt_workflow_sync_solr_collections($value, $collection_options);
    }

    if (!is_null($value)) {
      if (array_search($fieldname, $batch_integer_fields) === FALSE) {
        if ($fieldname == 'batch_request_due_date' || $fieldname == 'date') {
          $value = $value['year'] . '/' . $value['month'] . '/' . $value['day'];
        } elseif (is_array($value)) {
          $value = implode(",", $value);
        }
        // Don't add any update to file value if it is not set from previous batch editing.
        if (($fieldname == 'file' && $value) || $fieldname <> 'file') {
          $sql_values[] = "'" . mysqli_real_escape_string($link, $value) . "'";
        }
      } else {
        $sql_values[] = $value;
      }
    }
  }
  $sql_fields[] = 'user';
  $sql_values[] = $user->uid;

  $sql = "REPLACE INTO `batch` (`" . implode("`, `", $sql_fields) . "`) VALUES (" . implode(", ", $sql_values) . ")";

  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  $item_id = upitt_workflow_insert_item_for_batch_external_id($batch_external_id, $link);
  if (!$batch_id) {
    $batch_id = upitt_workflow_get_batch_id($batch_external_id);
  }

  upitt_workflow_relate_batch_to_item($batch_id, $item_id, $link);

  mysqli_close($link);
  if (is_null($valid_upload) && upitt_workflow_model_uses_csv_to_make_mods($model)) {
    drupal_set_message('There was no CSV file uploaded for this batch.', 'warning');
  }
  return $batch_id;
}

/**
 * This will first look up whether or not there is already a batch_item record that
 * matches this item, but is in the wrong batch it will delete that record ... 
 * else, if it does not find a record matching the item_id and batch_id, a new 
 * record will be created.
 *
 * @param integer $batch_id
 * @param integer $item_id
 * @param type $link
 */
function upitt_workflow_relate_batch_to_item($batch_id, $item_id, $link) {
  // 1. Look for a batch_item record that has the wrong batch associated with it...
  if ($item_id && $batch_id) {
    $sql = "SELECT id FROM batch_item WHERE item_id = $item_id AND batch_id <> $batch_id";
    $result = mysqli_query($link, $sql);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $sql);
    }
    $row = mysqli_fetch_assoc($result);
    if (isset($row['id'])) {
      $deleted = mysqli_query($link, "SELECT id FROM batch_item WHERE item_id = $item_id AND batch_id <> $batch_id");
      if (!$deleted) {
        $variables = array('@batch_id' => $batch_id, '@item_id' => $item_id);
        watchdog('upitt_workflow', 'There was a problem trying to delete the `batch_item` record for ' .
                 'batch_id = @batch_id and item_id = @item_id.', $variables, WATCHDOG_NOTICE);
      }
    }

    // 2. Look for the record with the correct values already, and add it if not found.
    $sql = "SELECT id FROM batch_item WHERE item_id = $item_id AND batch_id = $batch_id LIMIT 1";
    $result = mysqli_query($link, $sql);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $sql);
    }
    $row = mysqli_fetch_assoc($result);
    if (!isset($row['id'])) {
      $sql = "INSERT INTO batch_item (item_id, batch_id) VALUES (" . $item_id . ", " . $batch_id . ")";
      $result = mysqli_query($link, $sql);
      if (!$result) {
        upitt_workflow_sql_error_die($link, $sql);
      }
    }
  }
}

/**
 * This will look up the item record's id value - and create an item record if
 * needed.  This should ALWAYS be able to return an integer value.
 *
 * @param type $batch_external_id
 * @param type $link
 * @return integer the value of id field for the existing or new item record.
 */
function upitt_workflow_insert_item_for_batch_external_id($batch_external_id, $link) {
  // Update the item record for this do_id (*batch_external_id value)
  $sql = "SELECT id FROM item WHERE `do_id` = '" . $batch_external_id . "'";
  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }
  $row = mysqli_fetch_assoc($result);
  if (!isset($row['id'])) {
    upitt_workflow_log('item record did not exist');
    // If the passed $islandora_object is not null, use the label from that for this record.
    $label = (is_null($islandora_object) ? '' : $islandora_object->label);
    // Need to insert the item record.
    $insert_item = 'INSERT INTO item (do_id, name, type_id, property_owner_id) ' .
                   'VALUES ("' . upitt_workflow_safe_qstring($link, $batch_external_id) . '",' .
                   ' "' . upitt_workflow_safe_qstring($link, $label) . '", ' . $batch['type_id'] . ', ' . $batch['batch_property_owner_id'] . ')';
    mysqli_query($link, $insert_item);

    $sql = "SELECT id FROM item WHERE `do_id` = '" . $batch_external_id . "'";
    $result = mysqli_query($link, $sql);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $sql);
    }
    $row = mysqli_fetch_assoc($result);
  }
  return $row['id'];
}

function upitt_workflow_update_batch_nid($batch_id, $node) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $sql = 'UPDATE `batch` SET `nid` = ' . $node->nid . ' WHERE `batch_id` = ' . $batch_id;

  $result = mysqli_query($link, $sql);

  mysqli_close($link);
}

/**
 * Helper function that will set the value of a $node body to a block of HTML based on the batch record.
 * 
 * @param string $batch_external_id
 * @param string $batch_id
 * @param string $batch_description
 */
function upitt_workflow_node_body_for_batch($node, $batch_external_id, $batch_id, $batch_description, $extra_markup = '') {
  // TRUE / FALSE based on whether or not the node's body contained the text "/workflow/".
  $body_had_contents = ( isset($node->body) ? strstr($node->body['und'][0]['value'], '/workflow/') : FALSE);
  $body_markup = (($body_had_contents) ? $node->body['und'][0]['value'] :
      '<p>' .
          l(t('Edit'), '/workflow/batch/edit/' . $batch_id)) . ', ' .
          l(t('Ingest (prepare)'), '/workflow/batch/' . $batch_id . '/islandora_ingest') . ', or ' .
          l(t('Manage'), '/workflow/batch/' . $batch_id) . ' this batch.</p>' .
      (($batch_description) ? '<p><b>Description</b><br />' . $batch_description . '</p>' : '') .
      $extra_markup;
  $node->body['und'][0]['value'] = $body_markup;
  $node->body['und'][0]['summary'] = text_summary($body_markup);
  $node->body['und'][0]['format'] = 'full_html';
}

/**
 * Helper function that will return a drupal $node object - created from a batch record.
 * 
 * @param string $batch_external_id
 * @param string $batch_id
 * @param string $batch_description
 */
function upitt_workflow_make_node_for_batch($batch_external_id, $batch_id, $batch_description, $extra_markup = '') {
  global $user;
  $node = new stdClass();
  $node->type = 'workflow_batch';
  $node->language = 'en';
  node_object_prepare($node);

  $node->title = 'Workflow batch ' . $batch_external_id;
  upitt_workflow_node_body_for_batch($node, $batch_external_id, $batch_id, $batch_description, $extra_markup);
  $node->status = 1;   // (1 or 0): published or unpublished
  $node->promote = 0;  // (1 or 0): promoted to front page or not
  $node->sticky = 0;  // (1 or 0): sticky at top of lists or not
  $node->comment = 1;  // 2 = comments open, 1 = comments closed, 0 = comments hidden
  // Add author of the node
  $node->uid = $user->uid;
  // Set created date
  $current_date = date('H:i:s m/d/Y');
  $node->date = $current_date;
  $node->created = strtotime($current_date);

  $path = '/workflow_batch/' . $batch_external_id;
  $node->path = array('alias' => $path);

  $node = node_submit($node);
  node_save($node);
  drupal_set_message(l('Node created', '/node/' . $node->nid) . ' for workflow batch "' . $batch_external_id . '".  ' . l('Manage', '/workflow/batch/' . $batch_id) . ' this batch.');
  return $node;
}

function upitt_workflow_validate_csv($csv_filename, $batch_path) {
  $validation_success = TRUE;
  $csv_file_arr = upitt_workflow_csv_file_as_array($csv_filename);
  $file_header = (isset($csv_file_arr['header']) ? $csv_file_arr['header'] : array());
  $file_rows = (isset($csv_file_arr['rows']) ? $csv_file_arr['rows'] : array());

  // Get the normalized_date column index.
  $date_index = array_search('normalized_date', $file_header);

  // Get the filename column index.
  $filename_index = array_search('file_name', $file_header);
  // If no 'file_name' is not found, check the other field name 'filename'.
  if ($filename_index === FALSE) {
    $filename_index = array_search('filename', $file_header);
  }

  foreach ($file_rows as $k => $row) {
    // If there is a filename_index, check that the file exists.
    if (!$filename_index === FALSE) {
      $row_filename = $batch_path . '/' . $row[$filename_index];
      if (!file_exists($row_filename)) {
        $validation_success = FALSE;
        drupal_set_message(t('There is a filename referenced that does not exist on ' .
          'the file system.  "' . $row[$filename_index] . '" on row #' . $k . 
          '.  The file must be stored at "' . $batch_path . '/".  Please upload ' .
          'fix this issue to be able to ingest this batch.'), 'error');
      }
    }

    // If there is a normalized_date in the header, check this row's date value
    if (!($date_index === FALSE)) {
      $normalized_date = $row[$date_index];
      $check = upitt_workflow_make_custom_csv_dates($row[$date_index], TRUE);
      if ($check == '') {
        $validation_success = FALSE;
        drupal_set_message(t('There is a bad date value for "normalized_date" = "' .
          $row[$date_index] . '" on row #' . $k . '.  Please upload the CSV after ' .
          'fixing this value.'), 'error');
      }
    }
  }

  return $validation_success;
}

/**
 * Given a model name, returns whether or not a CSV file is needed to make the MODS
 * for a batch record.  Currently, this is only true for the image batches.
 *
 * @param string $model
 */
function upitt_workflow_model_uses_csv_to_make_mods($model) {
  return ($model == 'sp_large_image_cmodel');
}

/**
 * Will check the all_ingests.txt file as to whether or not the object identifer (barcode)
 * is in that file.
 *
 * @param string $id_no_namespace
 * @return Boolean
 */
function upitt_workflow_check_for_object_in_all_ingests($id_no_namespace) {
  // check against the "all_ingests.txt" file
  $all_ingests_file = file(drupal_get_path('module', 'upitt_workflow') . '/includes/all_ingests.txt');

  $found = FALSE;
  foreach ($all_ingests_file as $l) {
    if (!$found) {
      $found |= (trim($l) == $id_no_namespace);
    }
  }
  return $found;
}

/**
 * For the Audit object form - determines whether or not the page record object
 * exists in the islandora_workflow database.
 *
 * @param string $id_no_namespace
 * @return Boolean
 */
function upitt_workflow_does_workflow_page_record_exist($id_no_namespace) {
  @list($id_no_namespace, $page_number) = explode("-", $id_no_namespace);
  // look for a matching record in the workflow database
  $workflow_object_file_records = upitt_workflow_get_item_files($id_no_namespace);
  $workflow_page_record_exists = FALSE;
  $record_name = $page_number . '.tif';
  foreach ($workflow_object_file_records as $workflow_object_file) {
    if ($workflow_object_file['use'] == 'MASTER') {
      $workflow_page_record_exists |= ($workflow_object_file['name'] == $record_name);
    }
  }
  return $workflow_page_record_exists;
}

/**
 * This will look at the values in the Solr record for purposes of displaying a table of the datatream updates.
 *
 * @param array $solrRecord
 * @param string $pid
 * @return HTML markup.
 */
function upitt_workflow_analyze_solr_record($solrRecord, $pid) {
  // get all version, mimetype, filesize values for each datastream $dsid like fedora_datastream_version_MODS_CREATED_ms
  $rows = $headings = array();
  $datastreams = $solrRecord['fedora_datastreams_ms'];
  $headings = array('ID', 'version ID', 'LABEL', 'CREATED', 'MIMETYPE', 'SIZE', 'Operations');
  $messages = array();
  foreach ($datastreams as $dsid) {
    // the Size field may be an integer if the field has been added, or multistring by default.  We want to use
    // the integer field if it exists.
    $size_field_type = (isset($solrRecord['fedora_datastream_version_' . $dsid . '_SIZE_i']) ? '_i' : '_ms');

    $solrfield_ID = 'fedora_datastream_version_' . $dsid . '_ID_ms';
    $solrfield_LABEL = 'fedora_datastream_version_' . $dsid . '_LABEL_ms';
    $solrfield_CREATED = 'fedora_datastream_version_' . $dsid . '_CREATED_ms';
    $solrfield_MIMETYPE = 'fedora_datastream_version_' . $dsid . '_MIMETYPE_ms';
    $solrfield_SIZE = 'fedora_datastream_version_' . $dsid . '_SIZE' . $size_field_type;
    if (isset($solrRecord[$solrfield_ID]) && isset($solrRecord[$solrfield_LABEL]) && isset($solrRecord[$solrfield_CREATED]) && isset($solrRecord[$solrfield_MIMETYPE]) && isset($solrRecord[$solrfield_SIZE])) {
      foreach ($solrRecord[$solrfield_CREATED] as $key => $CREATED) {
        $pre = ((1+$key) == count($solrRecord[$solrfield_CREATED])) ? '<b title="current version">' : '';
        $post = ((1+$key) == count($solrRecord[$solrfield_CREATED])) ? '</b>' : '';
        $rows[$dsid] = array('ID' => ($pre) ? $pre . $dsid . $post : '',
          'version ID' => $pre . $solrRecord[$solrfield_ID][$key] . $post,
          'LABEL' => $pre . $solrRecord[$solrfield_LABEL][$key] . $post,
          'CREATED' => $pre . $solrRecord[$solrfield_CREATED][$key] . $post,
          'MIMETYPE' => $pre . $solrRecord[$solrfield_MIMETYPE][$key] . $post,
          'SIZE' => array('data' => $pre . number_format($solrRecord[$solrfield_SIZE][$key]) . $post, 'class' => array('numeric')),
          'Operations' => $pre . l('view', '/islandora/object/' . urlencode($pid) . '/datastream/' . $dsid . '/version/' . $key . '/view') . ' &nbsp; &nbsp; ' . l('revert', '/islandora/object/' . urlencode($pid) . '/datastream/' . $dsid . '/version/' . $key . '/revert') . $post,
        );
      }
      // If the SIZE field is not an array, update the value here
      if (!(is_array($solrRecord[$solrfield_SIZE]))) {
        $rows[$dsid]['SIZE'] = array('data' => $pre . number_format($solrRecord[$solrfield_SIZE]) . $post, 'class' => array('numeric'));
      }
    }
    else {
      $messages[] = '<div class="messages warning">One of the Solr fields is not set for datatream ' . $dsid . '.  ' . l(t('Manage datastreams'), '/islandora/object/' . urlencode($pid) . '/manage/datastreams'). '.</div>';
    }
  }

  ksort($rows);
  return implode($messages) . theme('table', array('rows'=>$rows,'header'=>$headings));
}

function upitt_workflow_get_depositor_model_and_datastreams_sizes($pid) {
  $fields = array();
  $datastreams = upitt_workflow_get_datastreams($pid);
  $size_field_multistring = $size_field_integer = '';
  foreach ($datastreams as $dsid => $datastream) {
    if (strstr($datastream, '(0)') == '') {
      $dsid =  strtoupper($dsid);
      $size_fields[] = 'fedora_datastream_version_' . $dsid . '_SIZE_i';
      $size_fields[] = 'fedora_datastream_version_' . $dsid . '_SIZE_ms';
      $size_field_multistring = ($size_field_multistring) ? $size_field_multistring : 'fedora_datastream_version_' . $dsid . '_SIZE_ms';
      $size_field_integer = ($size_field_integer) ? $size_field_integer : 'fedora_datastream_version_' . $dsid . '_SIZE_ms';
    }
  }

  // Add the depositor and hasModel fields to the fields array
  $fields[] = 'mods_name_depositor_namePart_s';
  $fields[] = 'RELS_EXT_hasModel_uri_s';
  
  module_load_include('inc', 'islandora_solr', 'includes/utilities');
  $query_processor = new IslandoraSolrQueryProcessor();

  $ret_arr = array();
  // run two queries --
  // 1) to get the datastreams available on this object
  $query_processor->solrQuery = 'PID:' . str_replace(array("/", ":", "-", "+"), array("\/", "\:", "\-", "\+"), $pid);
  $query_processor->solrStart = 0;
  $query_processor->solrLimit = 1;
  $query_processor->solrParams = array('fl' => implode(",", array_merge($size_fields, $fields)));

  $url = parse_url(variable_get('islandora_solr_url', 'localhost:8080/solr'));
  $solr = new Apache_Solr_Service($url['host'], $url['port'], $url['path'] . '/');
  $solr->setCreateDocuments(FALSE);
  try {
    $search_results = $solr->search($query_processor->solrQuery, $query_processor->solrStart, $query_processor->solrLimit, $query_processor->solrParams, 'GET');
    $tmp = json_decode($search_results->getRawResponse(), TRUE);

    $results = array();
    $numFound = $tmp['response']['numFound'];
    // repeatedly call this to process this many records each time:
    //   UPITT_ISLANDORA_INVENTORY_SOLR_CHUNKSIZE
    foreach ($tmp['response']['docs'] as $k=>$rec) {
      if (isset($rec[$size_field_integer]) && isset($rec[$size_field_multistring])) {
        foreach ($size_fields as $size_field) {
          if (strstr($size_field, '_ms')) {
            unset($rec[$size_field]);
          }
        }
      }
      $ret_arr = $rec;
    }
  }
  catch (Exception $e) {
    error_log('EXCEPTION in _save_solr_search_session : called from ' . $_SERVER['SERVER_NAME'] .
' - ' . $_SERVER['REQUEST_URI'] . '
' . print_r($e, true));
  }
  return $ret_arr;

  drupal_set_message('exiting upitt_islandora_inventory_audit_populate_islandora_pid');
}

function upitt_workflow_sum_datastream_sizes($datastream_sizes) {
  $latest = $total = 0;
  foreach ($datastream_sizes as $dsid => $datastream_sizes) {
    $i = 0;
    if (is_array($datastream_sizes)) {
      foreach ($datastream_sizes as $datastream_size) {
        if ($i < 1) {
          $latest = $latest + $datastream_size;
        }
        $i++;
        $total = $total + $datastream_size;
      }
    }
  }
  return array('datastreams_latestsize' => $latest,
               'datastreams_totalsize' => $total);
}

/**
 * Will look up the batch set for the given batch_external_id ... will match an
 * islandora PID value when prefixed with the ingest namespace variable.
 *
 * @param type $batch_external_id
 * @return integer - value of the set
 */
function upitt_workflow_get_set($batch_external_id) {
  $result = 0;
  $results = array();
  $q = db_select('islandora_batch_queue', 'q');
  $items = $q->fields('q', array('sid'))
    ->condition('q.id', upitt_workflow_get_ingest_namespace() . $batch_external_id)
    ->execute();
  while ($item = $items->fetchAssoc()) {
    $result = $item['sid'];
  }
  
  return $result;
}

/**
 * Update the batch status record for the matching a given $ingested_object.  This will 
 * create an item record if it does not exist already.
 *
 * @param object $ingested_object
 * @param integer $transaction_action_id
 */
function upitt_workflow_set_status_for_pid($ingested_object, $transaction_action_id, $allow_duplicates = FALSE) {
  global $user;
  // Take the namespace off of the pid to get the "batch_external_id" a.k.a. the "barcode" -- for finding the record in the workflow's item table.
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $batch_external_id = str_replace($ingest_namespace, "", $ingested_object->id);
  return upitt_workflow_set_status_for_batch($batch_external_id, $transaction_action_id, $ingested_object, $allow_duplicates);
}

/**
 * Update the batch status record for the matching batch record for this set's $ingested_pid.  This will
 * create an item record if it does not exist already.
 *
 * @param object $batch_external_id
 * @param integer $transaction_action_id
 */
function upitt_workflow_set_status_for_batch($batch_external_id, $transaction_action_id, $islandora_object = NULL, $allow_duplicates = TRUE) {
  global $user;
  $retval = TRUE; // Assume success, unless something something doesn't work, it'll set this to FALSE.
  upitt_workflow_log('in upitt_workflow_set_status_for_batch, batch_external_id = ' . $batch_external_id);

  // will need the batch record for this object.
  $batch_id = upitt_workflow_get_batch_id($batch_external_id, FALSE);
  if ($batch_id) {
    $batch = upitt_workflow_batch_load($batch_id);
  }
  else {
    $batch = array('type_id' => 0, 'batch_property_owner_id' => 0);
  }
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  // Get matching item record if there is one -- 
  $item_query = 'SELECT id FROM item i WHERE i.do_id = "' . upitt_workflow_safe_qstring($link, $batch_external_id) . '"';
  $result = mysqli_query($link, $item_query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $item_query);
  }
  upitt_workflow_log('search for batch item');
  // error_log('search for batch\'s item' . "\n", 3, '/tmp/log');

  $row = mysqli_fetch_assoc($result);
  if (!isset($row['id'])) {
    upitt_workflow_log('item record did not exist');
    // If the passed $islandora_object is not null, use the label from that for this record.
    $label = (is_null($islandora_object) ? '' : $islandora_object->label);
    // Need to insert the item record.
    $insert_item = 'INSERT INTO item (do_id, name, type_id, property_owner_id) ' . 
                   'VALUES ("' . upitt_workflow_safe_qstring($link, $batch_external_id) . '",' .
                   ' "' . upitt_workflow_safe_qstring($link, $label) . '", ' . $batch['type_id'] . ', ' . $batch['batch_property_owner_id'] . ')';
    mysqli_query($link, $insert_item);

    // Use the same query as before to get the item_id of the new record created above.
    $result = mysqli_query($link, $item_query);
    $row = mysqli_fetch_assoc($result);
  }
  
  // at this point, the $row['id'] better be set, but still need to check before inserting the transaction record.
  if (isset($row['id'])) {
    $item_id = $row['id'];
    upitt_workflow_log('item id = ' . $item_id);
    // error_log('item id = ' . $item_id . "\n", 3, '/tmp/log');

    $transaction_id = FALSE;
    $can_insert = TRUE;
    if (!$allow_duplicates) {
      // Look for an existing record - and set $can_insert based on whether or not the record existed.
      $transaction_query = 'SELECT id FROM `transaction` WHERE `item_id` = ' . $item_id . ' ORDER BY id DESC LIMIT 1';
      $result = mysqli_query($link, $transaction_query);
      $row = mysqli_fetch_assoc($result);
      if (isset($row['id'])) {
        $can_insert = FALSE;
        $transaction_id = $row['id'];
      }
    }
    if ($can_insert) {
      // Two records for the transaction ... insert the transaction record -- and update the item_current_status table with a pointer to this transaction
      $transaction_insert = 'INSERT INTO `transaction` (`item_id`, `user_id`, `transaction_action_id`, `timestamp`) ' .
                            'VALUES (' . $item_id . ', ' . $user->uid . ', ' . ($transaction_action_id + 0) . ', now())';
      mysqli_query($link, $transaction_insert);

      $transaction_query = 'SELECT id FROM `transaction` WHERE `item_id` = ' . $item_id . ' ORDER BY id DESC LIMIT 1';
      $result = mysqli_query($link, $transaction_query);
      $row = mysqli_fetch_assoc($result);
      if (isset($row['id'])) {
        $transaction_id = $row['id'];
      }
    }
    if ($transaction_id) {
      upitt_workflow_log('transaction_id = ' . $transaction_id);
      // error_log('transaction_id = ' . $transaction_id . "\n", 3, '/tmp/log');

      // FINALLY, put the transaction_id into the item_current_status table.
      $trans_acts_insert = 'INSERT INTO `item_current_status` (`item_id`, `last_transaction_id`) VALUES (' . $item_id . ', ' . $transaction_id . ')';
      $result = mysqli_query($link, $trans_acts_insert);
    }
  }
  else {
    upitt_workflow_log('item_id not found for ' . $item_query);
    $retval = FALSE;
  }
  mysqli_close($link);
  return $retval;
}

// TODO - finish this function and call 
function upitt_workflow_update_batch_file($batch_external_id, $local_name) {
  $sql = "UPDATE batch SET file = '" . $local_name . "' WHERE batch_external_id = '" . $batch_external_id . "'";
  dpm($sql);
}

/**
 * This will add the relationship specified by the predicate_uri, predicate
 * and relation and make sure that the relationship does not exist before adding it.
 *
 * @param fedora_object $object
 * @param string $predicate_uri
 * @param string $predicate
 * @param type $relation
 * @param type $type
 */
function upitt_workflow_addRelationship($object, $predicate_uri, $predicate, $relation, $type) {
  // get the current relationships
  $rels = $object->relationships->get($predicate_uri, $relation);
  $existed = FALSE;
  foreach ($rels as $rel) {
    $existed |= ((isset($rel['object']['value']) && $rel['object']['value'] == $relation) && (isset($rel['predicate']['value']) && $rel['predicate']['value'] == $predicate));
  }
  if (!$existed) {
    $relationships = $object->relationships;
    $relationships->add($predicate_uri,$predicate,$relation,$type);
  }
  return !$existed;
}

/**
 * Used by the process_barcodes form as well as the "do_publish" form to set the 
 * relationships between the object and the collections / sites that are provided.
 * 
 * @param type $object_id
 * @param type $sites_arr
 * @param type $collections_arr
 */
function upitt_workflow_relate_object_to_collections_sites($object_id, $sites_arr, $collections_arr, $raw_pid = FALSE) {
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  if (count($sites_arr) > 0 || count($collections_arr) > 0) {
    // 1. load the islandora_object - and loop through collections and sites to add
    // these relationships using the upitt_workflow_addRelationship - which will ensure
    // that duplicate relationships are not added.
    if (!$raw_pid) {
      $pid = $ingest_namespace . $object_id;
    }
    else {
      $pid = $object_id;
    }
    $object = islandora_object_load($pid);
    if (is_object($object)) {
      // additionally, remove this object from any review collection like "pitt:collection.##_review"
      $isMemberOfCollection = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOfCollection');
      foreach ($isMemberOfCollection as $memberOfCollection) {
        if (rtrim($memberOfCollection['object']['value'], '_review') <> $memberOfCollection['object']['value']) {
          $object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $memberOfCollection['object']['value']);
        }
      }

      foreach ($sites_arr as $site) {
        if (trim($site)) {
          $object->relationships->remove('http://digital.library.pitt.edu/ontology/relations#', 'isMemberOfSite', $site);
          $object->relationships->add('http://digital.library.pitt.edu/ontology/relations#', 'isMemberOfSite', $site);
        }
      }

      foreach ($collections_arr as $collection) {
        if (trim($collection)) {
          $object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection);
          $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection);
        }
      }

      upitt_workflow_set_status_for_batch($object_id, UPITT_WORKFLOW_ACTION_OBJECT_PUBLISHED_ON_SITE, NULL, TRUE);
      drupal_set_message('Published "' . $object_id . '" to sites and collections.');
    }
    else {
      drupal_set_message('Could not load object ' . $pid, 'warning');
    }
  }
  else {
    drupal_set_message('There were no sites or collections related to the object "' . $object_id . '"', 'warning');
  }
}

function upitt_workflow_object_isPage($islandora_object) {
  $applicable_cmodels = array('islandora:manuscriptPageCModel','islandora:pageCModel','islandora:newspaperPageCModel');
  return (is_object($islandora_object)) ? array_intersect($applicable_cmodels, $islandora_object->models) : FALSE;
}

/**
 * This will return the node values that match the xpath query -- and potentially
 * filter to return results that match a given string value.
 *
 * @param type $xml_filename
 * @param type $xpath_query
 * @param type $dom_namespaces
 * @param type $matching_value_filter
 * @return array
 */
function upitt_workflow_get_xpath_node_values_from_xml($xml_filename, $xpath_query, $dom_namespaces, $matching_value_filter = '') {
  if (!file_exists($xml_filename)) {
    return;
  }
  $f = file_get_contents($xml_filename);
  $doc = new DOMDocument();
  $doc->loadXML($f);
  $xpath = new DOMXPath($doc);
  foreach ($dom_namespaces as $ns_name => $ns) {
    $xpath->registerNamespace($ns_name, $ns);
  }

  $ret = array();
  $results = $xpath->query($xpath_query);
  $nodeValue = '';
  foreach ($results as $result) {
    $nodeValue = htmlspecialchars(trim($result->nodeValue));
    if ($matching_value_filter && strstr($nodeValue, $matching_value_filter)) {
      $ret[] = $nodeValue;
    } elseif (!$matching_value_filter) {
      $ret[] = $nodeValue;
    }
  }
  return $ret;
}

/**
 * This will query Solr to find the PID of any object that has this OCoLC value
 * for the mods_identifier_oclc_s field.
 *
 * @param type $OCoLC
 */
function upitt_workflow_get_PID_of_oclc($OCoLC) {
  $ret = upitt_workflow_get_solr_options('mods_identifier_oclc_s:' . ($OCoLC ? $OCoLC : '*'), 'PID', 'PID');
  return array_pop($ret);
}

function upitt_workflow_formatBytes($bytes, $precision = 2) {
    $units = array('B', 'KB', 'MB', 'GB', 'TB');

    $bytes = max($bytes, 0);
    $pow = floor(($bytes ? log($bytes) : 0) / log(1024));
    $pow = min($pow, count($units) - 1);

    // Uncomment one of the following alternatives
    // $bytes /= pow(1024, $pow);
    $bytes /= (1 << (10 * $pow));

    return round($bytes, $precision) . ' ' . $units[$pow];
}

function upitt_workflow_get_ingest_temp_space() {
  $cmd = 'df -P | gawk \'{ printf "%s\t%s\t%s\t%s\t%s\n", $1, $6, $2, $3, $4 }\'';
  $output = array();
  exec($cmd, $output, $return_var);
  $temp_dir = file_directory_temp();
  if (strstr($temp_dir, 'ingest') == '') {
    $temp_dir = '/';
  }

  $found_line = '';
  $space = 0;
  foreach ($output as $line) {
    $line_parts = explode("\t", $line);
    $line_path =  isset($line_parts[1]) ? $line_parts[1] : $line_parts[0];
    if (!$found_line && ($line_path == $temp_dir)) { // (strstr($line, $temp_dir) <> '')) {
      $found_line = $line;
    }
  }
  if ($found_line) {
    $r = explode("\t", $found_line);
    // since the number represents the # of 1k blocks, this must be multiplied by 1024
    // to get the # of bytes.
    $space = isset($r[4]) ? $r[4] * 1024 : 0;
  }
  return $space;
}

/**
 * This will inspect the filenames that are supplied in the $files array and
 * return the fedora model that the files represent.
 *
 * @param array $files
 * @return string
 *   The name of the model that the files likely represent
*/
function upitt_workflow_determine_fedora_model_from_filenames($files) {
  $extensionToContentModelMap = array(
    'jpeg' => 'islandora:sp_basic_image',
    'jpg' => 'islandora:sp_basic_image',
    'gif' => 'islandora:sp_basic_image',
    'png' => 'islandora:sp_basic_image',
    'tif' => 'islandora:sp_large_image_cmodel',
    'tiff' => 'islandora:sp_large_image_cmodel',
    'jp2' => 'islandora:sp_large_image_cmodel',
    'pdf' => 'islandora:sp_pdf',
    'mp3' => 'islandora:sp-audioCModel',
    'mp4a' => 'islandora:sp-audioCModel',
    'm4a' => 'islandora:sp-audioCModel',
    'oga' => 'islandora:sp-audioCModel',
    'ogg' => 'islandora:sp-audioCModel',
    'flac' => 'islandora:sp-audioCModel',
    'wav' => 'islandora:sp-audioCModel',
    'mp4' => 'islandora:sp_videoCModel',
    'm4v'  => 'islandora:sp_videoCModel',
    'mkv'  => 'islandora:sp_videoCModel',
    'mpeg' => 'islandora:sp_videoCModel',
    'mpe' => 'islandora:sp_videoCModel',
    'mpg' => 'islandora:sp_videoCModel',
    'qt' => 'islandora:sp_videoCModel',
    'mov' => 'islandora:sp_videoCModel',
    'ogv' => 'islandora:sp_videoCModel',
  );
  $model = 'sp_large_image_cmodel';
  foreach ($files as $file) {
    $file_pathinfo = pathinfo($file);
    if (isset($file_pathinfo['extension'])) {
      $lowcase_extension = strtolower($file_pathinfo['extension']);
      if (!(array_key_exists($lowcase_extension, $extensionToContentModelMap) === FALSE)) {
        $model = $extensionToContentModelMap[$lowcase_extension];
      }
    }
  }
  return $model;
}

/**
 * This will return the fedora model name of a given file extension.
 *
 * @param string $extension
 * @return string
 */
function upitt_workflow_determine_fedora_model_from_extension($extension) {
  // Tricky way to use the existing function... just make up a dummy filename
  // to use for that function call... it is the extension that is important.
  $files = array('0' => 'tmp.' . $extension);
  return upitt_workflow_determine_fedora_model_from_filenames($files);
}