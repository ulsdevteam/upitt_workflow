<?php

/**
 *  get_config_value
 */
function get_config_value($section,$key) {
  if ( file_exists('/usr/local/src/islandora_tools/uls-workflow.ini') ) {
    $ini_array = parse_ini_file('/usr/local/src/islandora_tools/uls-workflow.ini', true);
    if (isset($ini_array[$section][$key])) {
      $value = $ini_array[$section][$key];
      return ($value);
    } else {
      return ("");
    }
  } else {
    return(0);
  }
}

/** 
 * Simple lookup function to provide mapping between the legacy workflow "type_of_resource" to the Islandora fedora model name.
 */
function upitt_workflow_get_workflow_to_fedora_object_mappings() {
  return array('still image' => 'sp_large_image_cmodel',
               'photograph' => 'sp_large_image_cmodel',
               'manuscript' => 'manuscriptCModel',
               'text' => array('bookCModel', 'newspaperCModel'),
              );
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
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
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
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
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
  $query_processor->solrParams = array('fl' => $key_field . ',' . $value_field);

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
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
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
      $retvals[] = $collections[$id]['c_id'];
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
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
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
    $sql = "INSERT INTO `collection` (`c_id`, `PID`, `name`) VALUES ('". $pid . "', '". $pid . "', '" . upitt_workflow_safe_qstring($link, $name) . "')";
    $result = mysqli_query($link, $sql);
    if (!$result) {
      $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
      $message .= 'Whole query: ' . $query;
      die($message);
    }
    drupal_set_message('added collection workflow record reference for "' . $name . '"');
  }
  mysqli_close($link);
}

function upitt_workflow_get_allowed_listnames() {
  return array('action', 'collection', 'content_types', 'item_type', 'property_owner', 'sites', 'workflow_sequence', 'workflow_sequence_actions');
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
  $query = 'SELECT t.description, t.`timestamp` `time` ' .
           'FROM item i ' .
           'JOIN `transaction` t ON (t.item_id = i.id) ' .
           'WHERE i.do_id="' . upitt_workflow_safe_qstring($link, $object_id) . '" ' .
           'ORDER BY t.`timestamp` ASC';

  $result = mysqli_query($link, $query);
  if (!$result) {
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
  }

  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[] = $row;
  }
  mysqli_close($link);
  return $rows;
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
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
  }

  $row = mysqli_fetch_assoc($result);

  mysqli_close($link);
  return $row;
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
    $message  = 'Invalid query: ' . mysqli_error($link) . "\n";
    $message .= 'Whole query: ' . $query;
    die($message);
  }

  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[] = $row;
  }
  mysqli_close($link);
  return $rows;
}

/**
 * This function will return the Solr record for this object so that it can be compared to the expected values from the workflow record;
 */
function upitt_workflow_get_solr_record($pid) {
  module_load_include('inc', 'islandora_solr_search', 'includes/utilities');
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
