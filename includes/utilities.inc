<?php

/**
 *  get_config_value
 */
function get_config_value($section,$key)
{
    if ( file_exists('/usr/local/src/islandora_tools/uls-workflow.ini') )
    {
        $ini_array = parse_ini_file('/usr/local/src/islandora_tools/uls-workflow.ini', true);
        if (isset($ini_array[$section][$key]))
        {
            $value = $ini_array[$section][$key];
            return ($value);
        } else {
            return ("");
        }
    } else {
        return(0);
    }
}

function upitt_workflow_get_mysql_options($table_name, $option_name = '', $key_field, $value_field, $sort_field = '', $option_value = 0) {
  $query = 'SELECT ' . $key_field . ', ' . $value_field . ' FROM ' . $table_name . ($sort_field ? ' ORDER BY ' . $sort_field : '');
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $result = mysqli_query($link, $query); // mysql_query($query, $link);
  if (!$result) {
    $message  = 'Invalid query: ' . mysql_error() . "\n";
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
  $query_processor->solrParams = array(
    'fl' => $key_field . ',' . $value_field);

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
