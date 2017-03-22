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
               'text' => 'bookCModel',
               'cartographic' => 'sp_large_image_cmodel',
               'newspaper' => 'newspaperCModel',
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

function upitt_workflow_get_models() {
  form_load_include($form_state, 'inc', 'islandora', 'includes/content_model.autocomplete');

  // Retrieve all content models and unset those currently in use in this
  // configuration and any others from other configurations.
  $add_options = islandora_get_content_model_names();
  return $add_options;
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
    drupal_set_message('Replaced collection workflow record reference for "' . $name . '"');
  }
  mysqli_close($link);
}

function upitt_workflow_get_allowed_listnames() {
  return array('action', 'collection', 'content_types', 'item_type', 'property_owner', 'wflocal_fedora_site', 'workflow_sequence', 'workflow_sequence_actions');
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
  $query = 'SELECT ta.description, t.`timestamp` `time` ' .
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
 * Will return just item file record for the given object_id (legacy :do_id value).
 */
function upitt_workflow_get_item($object_id) {
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');
  $query = 'SELECT i.id, i.do_id, i.name, i.type_id, i.property_owner_id, i.primary_collection_id, ' .
           ' (SELECT GROUP_CONCAT(lfc.pid) ' . 
           ' FROM wflocal_local_item_fedora_collections lifc ' . 
           ' JOIN wflocal_fedora_collection lfc ON (lfc.id = lifc.fedora_collection_id) ' .
           ' WHERE lifc.local_item_id = i.id) `collections`, ' .
           ' (SELECT GROUP_CONCAT(lfs.pid) ' .
           ' FROM wflocal_local_item_fedora_sites lifs ' .
           ' JOIN wflocal_fedora_site lfs ON (lfs.id = lifs.fedora_site_id) ' .
           ' WHERE lifs.local_item_id = i.id) `sites` ' .	
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
  die($message);
}

/** DEPRECATED
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

/** DEPRECATED
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
 * Helper function to return a set of filenames from a given path -- with the option to filter ($mask)
 * and an option to recursively ($recurse) search for files under the initial directory path ($dir).
 */
function upitt_workflow_file_scan_directory($dir, $mask = '', $recurse = FALSE) {
  $result = array();

  $cdir = scandir($dir);
  foreach ($cdir as $key => $value) {
    if (!in_array($value,array(".",".."))) {
      if ($recurse && (is_dir($dir . DIRECTORY_SEPARATOR . $value))) {
        $result[$value] = upitt_workflow_file_scan_directory($dir . DIRECTORY_SEPARATOR . $value, $mask, $recurse);
      }
      else {
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
function upitt_workflow_make_custom_csv_dates($short_date) {
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
        die('bad date format for date value of "' . $short_date . '".');
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
      die('bad date format for date value of "' . $short_date . '".');
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
  return array('genre'                     => '/mods:mods/mods:genre',
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
               // These field values are not represented in a MODS file, but will appear in the CSV file
	       'batch' => '',
               'collection' => '',
               // 'date_qualifier' => '',
               'file_name' => '',
               'filename' => '',
               'location' => '',
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
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

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

function upitt_workflow_batch_load($batch_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

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
function upitt_workflow_get_datastreams() {
  $query_processor = new IslandoraSolrQueryProcessor();
  $query_processor->solrQuery = '*:*';
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
 * @param type $csv_file
 * @return array('header' => $file_header, 'rows' => $file_rows)
 * @throws Exception
 */
function upitt_workflow_csv_file_as_array($csv_file) {
  $file_rows = $file_header = array();
  if (file_exists($csv_file)) {
    $row = 0;
    if (($handle = fopen($csv_file, "r")) !== FALSE) {
      try {
        while (($data = fgetcsv($handle)) !== FALSE) {
          $row++;
	  if ($row < 50) {
            if ($row > 1) {
              $file_rows[] = $data;
            }
            else {
              $tmp_data = $data;
              foreach ($tmp_data as $i => $val) {
                $tmp_data[$i] = strtolower($val);
              }
              $file_header = $tmp_data;
            }
          }
        }
        if ($row > 50) {
          // Subtract the header row -
          $data_rows = $row - 1;
          drupal_set_message('There are ' . number_format($data_rows) . ' rows in the CSV file "' . $csv_file . '".  Only the first 50 rows are displayed.', 'warning');
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
 * Helper function that will return the pid values from a spreadsheet.  This will
 * need to inspect the header column to find out which index represents the
 * identifier field - and then return that column of data from the rows.
 *
 * @param type $csv_arr  ('header' => $file_header, 'rows' => $file_rows)
 */
function upitt_workflow_csv_extract_identifiers($csv_arr) {
  $identifier_key = array_search('identifier', $csv_arr['header']);
  $ns_prefix = '';
  $ret_arr = array();
  if (!$identifier_key === FALSE) {
    foreach ($csv_arr['rows'] as $row) {
      $ret_arr[] = ((strstr($row[$identifier_key], 'pitt:') == '') ? 'pitt:' : '') . $row[$identifier_key];
    }
  }
  return $ret_arr;
}

function upitt_workflow_get_pids_solr_query($solr_query) {
  return implode('
', upitt_workflow_get_pids($solr_query));
}

function upitt_workflow_get_pids_collection($collection) {
  $query = format_string('!field:!value', array(
    '!field' => 'RELS_EXT_isMemberOfCollection_uri_ms',
    '!value' => str_replace(array("/", ":", "-", "+"), array("\/", "\:", "\-", "\+"), $collection),
   ));
  return implode('
', upitt_workflow_get_pids($query));
}

function upitt_workflow_get_pids_model($model) {
  $query = format_string('!field:!value', array(
    '!field' => 'RELS_EXT_hasModel_uri_ms',
    '!value' => str_replace(array("/", ":", "-", "+"), array("\/", "\:", "\-", "\+"), $model),
   ));
  return implode('
', upitt_workflow_get_pids($query));
}

function upitt_workflow_get_pids($query) {
  module_load_include('inc', 'islandora_solr_search', 'includes/utilities');
  $query_processor = new IslandoraSolrQueryProcessor();

  $ret_arr = array();
  // run two queries --
  // 1) to get the datastreams available on this object
  $query_processor->solrQuery = $query;
  $query_processor->solrStart = 0;
  $query_processor->solrLimit = UPITT_WORKFLOW_SOLR_LIMIT;
  $query_processor->solrParams = array('fl' => 'PID');

  $url = parse_url(variable_get('islandora_solr_url', 'localhost:8080/solr'));
  $solr = new Apache_Solr_Service($url['host'], $url['port'], $url['path'] . '/');
  $solr->setCreateDocuments(FALSE);
  try {
    $search_results = $solr->search($query_processor->solrQuery, $query_processor->solrStart, $query_processor->solrLimit, $query_processor->solrParams, 'GET');
    $tmp = json_decode($search_results->getRawResponse(), TRUE);

    $results = array();
    $numFound = $tmp['response']['numFound'];
    if ($numFound == UPITT_WORKFLOW_SOLR_LIMIT) {
      drupal_set_message(t('The number of objects returned is the same as the maximum limit.  It is likely that there are more objects than ' . UPITT_WORKFLOW_SOLR_LIMIT));
    }
    
    if ($tmp['response']['numFound'] > 0) {
      foreach ($tmp['response']['docs'] as $k=>$rec) {
        $ret_arr[] = $rec['PID'];
      }
    }
  }
  catch (Exception $e) {
    error_log('EXCEPTION in _save_solr_search_session : called from ' . $_SERVER['SERVER_NAME'] .
' - ' . $_SERVER['REQUEST_URI'] . '
' . print_r($e, true));
  }
  return $ret_arr;
}

/**
 * This will return the ingest path for a given batch by batch_external_id.  If the folder
 * does not exist, it will be created.  It will be named 'batch_{$batch_external_id}'.
 *
 * @param type $batch_external_id
 * @return type
 */
function upitt_workflow_batch_path($batch_external_id) {
  if (!$batch_external_id) {
    return '';
  }
  $ingest_path = variable_get('upitt_workflow_ingest_path', '/ingest/incoming');
  $batch_path = rtrim($ingest_path, '/') . '/batch_' . $batch_external_id;
  if (!file_exists($batch_path)) {
    mkdir($batch_path);
  }
  return (file_exists($batch_path)) ? $batch_path : '';
}

/**
 * This will return the export path for a given batch by batch_external_id and
 * optional datastream id.  If the folder does not exist, it will be created.
 * It will be named 'batch_{$dsid_}{$batch_external_id}'.
 *
 * @param type $batch_external_id
 * @return type
 */
function upitt_workflow_batch_export_path($batch_external_id, $dsid = '') {
  $export_path = variable_get('upitt_workflow_export_path', '/ingest/incoming');

  $batch_path = rtrim($export_path, '/') . '/batch_' . ($dsid ? $dsid . '_' : '') . $batch_external_id;
  if (!file_exists($batch_path)) {
    mkdir($batch_path);
  }
  return (file_exists($batch_path)) ? $batch_path : '';
}
