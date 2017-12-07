#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include "graph.h"
#include "schema.h"
#include "tuple.h"
#include "cli.h"
#include "vector.h"
#include <math.h>
#include <stdlib.h>
#include <unistd.h> //used for read and lseek
#include <limits.h> //Used to hold infinity.
#define _DEBUG 1
#define sizeof_attribute_name sizeof(c->se->attrlist->name)
#define MIN (X,Y) ((X) < (Y) ? : (X) : (Y))

void tests();

vertexid_t amin(vertexid_t a, vertexid_t b)
{
  if(a<b) return a;
  else return b;
}

vertexid_t get_edge_weight(component_t c, vertexid_t v1, vertexid_t v2, char attr_name[]){
  //Function takes in the vertex id's that represent th edges./
  //Returns -1 if the edge is not found,
  struct edge e;
  edge_t e1;
  int offset, i;

  edge_init(&e);
  edge_set_vertices(&e, v1, v2);
  e1 = component_find_edge_by_ids(c,&e);

  if (e1 == NULL)
  {
    return INT_MAX; //limits.h
  }
  offset = tuple_get_offset(e1->tuple, attr_name);
  i = tuple_get_int(e1->tuple->buf + offset);
  return i;
}

vertexid_t *get_vertexids(component_t c, int vertex_counter)
{
  //Returns an array of the vertexids found inside componenet.
  vertexid_t *vertexlist = malloc(vertex_counter * sizeof(vertexid_t));
  vertex_counter = 0;
  off_t off;
	ssize_t len, size;
	vertexid_t id;
	char *buf;
	int readlen;

  if (c->sv == NULL)
		size = 0;
	else
		size = schema_size(c->sv);

	readlen = sizeof(vertexid_t) + size;
	buf = malloc(readlen);

	for (off = 0;; off += readlen) {
		lseek(c->vfd, off, SEEK_SET);
		len = read(c->vfd, buf, readlen);
		if (len <= 0)
			break;

		id = *((vertexid_t *) buf);
    vertexlist[vertex_counter] = id; //add the vertexes to the vertext list
    vertex_counter++;
  }
  for(int i = 0; i < vertex_counter+1; i++){
    printf("TEH ID WHEN GETTING VERTEX IDs:%llu\n",vertexlist[i]);
  }
  free(buf);
  return vertexlist; //return the newly built vertex list.
}
int count_num_verticies(component_t c)
{
  //Counts the number of vertecies found in the componenet.
  int vertex_counter = 0;
  off_t off;
	ssize_t len, size;
	vertexid_t id;
	char *buf;
	int readlen;

  if (c->sv == NULL)
		size = 0;
	else
		size = schema_size(c->sv);

	readlen = sizeof(vertexid_t) + size;
	buf = malloc(readlen);

	for (off = 0;; off += readlen) {
		lseek(c->vfd, off, SEEK_SET);
		len = read(c->vfd, buf, readlen);
		if (len <= 0)
			break;

		id = *((vertexid_t *) buf);
    vertex_counter++;
  }
  return vertex_counter;
  free(buf);
}
int get_integer_attribute_name(component_t c, char *attrname)
{
  //Returns the first name of the integer attribute.
  struct attribute *cur_addr = c->se->attrlist;
  while(1)
  {
    printf("Before attrname call\n");
    if(cur_addr->bt == INTEGER)
    {
      // #if _DEBUG
        printf("Integer type found in edgelist schema!\n");
        printf("Integer attribute name: [%s]\n", cur_addr->name);
      // #endif

      // char attrname[sizeof((char*)cur_addr->name)];
      memcpy(attrname, cur_addr->name, sizeof_attribute_name);
      assert(cur_addr->bt == INTEGER); //Sanity checking that we have the right value
      return 1;
    }
    else if (cur_addr->next == NULL)
    {
      printf("CRITIAL ERROR: Could not find schema with an typeof INTEGER on edge.\n");
      printf("There must be at least one attribute with typeof integer to use as weight.\n");
      return -1;
    }
    cur_addr = cur_addr->next;
  }
}

vertexid_t *get_min_compare_vertex_lists(vertexid_t *array1, vertexid_t *array2, int number_of_verticies)
{
  vertexid_t *temp_list = malloc(number_of_verticies * sizeof(vertexid_t));
  //Initialize temp_list to ULONGLONG MAX values.
  for(int i = 0; i < number_of_verticies; i++){
    temp_list[i] = INT_MAX;
  }
  //Preform V-S here...
  int same_counter = 0;
  for(int i = 0;i < number_of_verticies; ++i)
  {
    int is_the_same = 0;
    for(int ii = 0; ii < number_of_verticies; ++ii)
    {
      if(array1[i] == array2[ii])
      {
        is_the_same = 1;
        break;
      }
    }
    if(!is_the_same)
    {
      temp_list[i] = array1[i];
      same_counter++;
    }
  }
  return temp_list;
}

vertexid_t find_min_in_array(vertexid_t *temp_list, int number_of_verticies)
{
  vertexid_t min = INT_MAX;
  int index = -1;
  for(int i = 0;i<number_of_verticies;i++){
    if(temp_list[i] <= min)
    {
      min = temp_list[i];
      index = i;
    }
  }
  assert(index != -1);
  return index;
}

int
component_sssp(
        component_t c,
        vertexid_t starting_vertex,
        vertexid_t ending_vertex,
        int *n,
        int *total_weight,
        vertexid_t **path)
{
  //Preform some simple tests to ensure helper functions work properly.
  // tests();
  //Set efd and vfd.
  c->efd = edge_file_init(gno, cno);
  c->vfd = vertex_file_init(gno,cno);
	/*
	 * Figure out which attribute in the component edges schema you will
	 * use for your weight function
	*/

  // Creares attribute name with type of integer
  // And check to see if result is valid.
  char attribute_name[sizeof_attribute_name];
  if(get_integer_attribute_name(c, attribute_name) <= 0)
    { return -1; }
    int weight = get_edge_weight(c, 1, 1, attribute_name);

  //count the number of verticies found in the graph.
  int number_of_verticies = count_num_verticies(c);
  printf("number of vertices: %d\n", number_of_verticies);
  //Initialize arrays for Dijkstra's.
  vertexid_t* parent_array    = malloc(number_of_verticies * sizeof(vertexid_t));
  vertexid_t* vertex_list     = get_vertexids(c, number_of_verticies);
  vertexid_t* d_cost_array    = malloc(number_of_verticies * sizeof(vertexid_t));
  vertexid_t* V_minus_S_array;

  for(int i = 0; i < number_of_verticies; i++){
    printf("vertex list:%llu\n", vertex_list[i]);
    parent_array[i] = INT_MAX;
    d_cost_array[i] = INT_MAX;
  }

  for(int i = 1; i < number_of_verticies; i++){
      int temp_weight = get_edge_weight(c, starting_vertex, vertex_list[i], attribute_name);
      if(temp_weight != INT_MAX){
          parent_array[i] = starting_vertex;
      }

  }

  //Suplementary Dijkstra variables.
  vertexid_t v = 0;
  vertexid_t w = 0;
  int s_array_ctr = 0;

  for(int i = 1;i < number_of_verticies;i++){
    //D[i] = C[1,i]
    d_cost_array[i] = get_edge_weight(c, starting_vertex, vertex_list[i], attribute_name);

    //start/clear s_array.
    vertexid_t* s_array = malloc(number_of_verticies * sizeof(vertexid_t));
    for(int i = 0; i < number_of_verticies; i++){
      s_array[i] = 0;
    }
    s_array[0] = starting_vertex;
    s_array_ctr = 1;

    printf("C[%llu,%llu]=%llu\n",w,v,get_edge_weight(c,vertex_list[w],vertex_list[v], attribute_name) );
    printf("INSIDE For LOOP1 w={%llu}v={%llu}:\n",w,v);
    printf("d_cost_arrray :: [");
    for(int n = 0; n < number_of_verticies; n++){
      printf("%llu,", d_cost_array[n]);
    }
    printf("]\n");
    printf("s_array: [");
      for(int n = 0; n < number_of_verticies; n++){
        printf("%llu,", s_array[n]);
      }
    printf("]\n");
    // getchar();


    for(int ii = 0; ii < number_of_verticies; ii++){
      //chose a vertex w in V-S s.t. D[w] is a minimum;
      //add w to S_array.
      printf("vertex_list :: [");
      for(int n = 0; n < number_of_verticies; n++){
          printf("%llu,", vertex_list[n]);
      }
      printf("]\n");
      printf("s_array :: [");
      for(int n = 0; n < number_of_verticies; n++){
          printf("%llu,", s_array[n]);
      }
      printf("]\n");

      //This will return the array V-S
      V_minus_S_array = get_min_compare_vertex_lists(vertex_list, s_array, number_of_verticies);

      w = find_min_in_array(V_minus_S_array, number_of_verticies);
      s_array[s_array_ctr] = vertex_list[w];
      s_array_ctr++;

      printf("INSIDE For LOOP2 w={%llu}v={%llu}:\n",w,v);
      printf("V_minus_S_array :: [");
      for(int n = 0; n < number_of_verticies; n++){
        printf("%llu,", V_minus_S_array[n]);
      }
      printf("]\n");
      printf("d_cost_arrary :: [");
      for(int n = 0; n < number_of_verticies; n++){
          printf("%llu,", d_cost_array[n]);
      }
      printf("]\n");
      printf("s_array :: [");
        for(int n = 0; n < number_of_verticies; n++){
          printf("%llu,", s_array[n]);
        }
      printf("]\n");
      // getchar();
      free(V_minus_S_array);


      for(int iii = 0; iii < number_of_verticies; iii++)
      {
        //D[w] := min(D[v], D[w] + C[w,v])
        // iii is the same as v. ??? (Is this true?)
        V_minus_S_array = get_min_compare_vertex_lists(vertex_list, s_array, number_of_verticies);
        v = iii;
        if(V_minus_S_array[v] >= 10000) {
          free(V_minus_S_array);
          continue;
         } //V-S[iii] spot is empty. No value for V-S

        // printf("\n\nd_cost_arrayiii:%llu\n", d_cost_array[iii]);
        printf("D[%d]=%llu\n",v,d_cost_array[v]);
        printf("D[%d]=%llu + ",w,d_cost_array[w]);
        printf("C[%llu,%llu]=%llu\n",w,v,get_edge_weight(c,vertex_list[w],vertex_list[v], attribute_name) );
        printf("Added: %llu",(d_cost_array[w] + get_edge_weight(c,vertex_list[w],vertex_list[v], attribute_name)));

        if((d_cost_array[w] + get_edge_weight(c,vertex_list[w],vertex_list[v], attribute_name) ) < d_cost_array[v])
        {
          parent_array[v] = vertex_list[w];
        }
        d_cost_array[v] = amin(d_cost_array[v], (d_cost_array[w] + get_edge_weight(c,vertex_list[w],vertex_list[v], attribute_name) ));


        printf("INSIDE For LOOP3 w={%llu}v={%llu}:\n",w,v);
        printf("V_minus_S_array :: [");
        for(int n = 0; n < number_of_verticies; n++){
          printf("%llu,", V_minus_S_array[n]);
        }
        printf("]\n");
        printf("d_cost_array :: [");
        for(int n = 0; n < number_of_verticies; n++){
          printf("%llu,", d_cost_array[n]);
        }
        printf("]\n");
        printf("s_array :: [");
          for(int n = 0; n < number_of_verticies; n++){
            printf("%llu,", s_array[n]);
          }
        printf("]\n");
        // getchar();
      }
    }
  }
  parent_array[0] = 0;
  printf("vertex_list :: [");
    for(int n = 0; n < number_of_verticies; n++){
      printf("%llu,", vertex_list[n]);
    }
  printf("]\n\n");
  printf("vertex_list :: [");
    for(int n = 0; n < number_of_verticies; n++){
      printf("%llu,", parent_array[n]);
    }
  printf("]\n");

  // for(int i = 0; i < number_of_verticies; i++)
  // {
  //   printf("Parent Array: %llu\n",parent_array[i]);
  // }
  // for(int i = 0; i < number_of_verticies; i++)
  // {
  //   printf("d_cost Array: %llu\n",d_cost_array[i]);
  // }




	/*
	 * Execute Dijkstra on the attribute you found for the specified
	 * component
	 */



	/* Change this as needed */
  #if _DEBUG
    printf("\n#################GRACEFULLY ENDING SSSP HERE#################\n");
  #endif
  // free(s_array);
  free(parent_array);
  free(vertex_list);
  // free(V_minus_S_array);
  free(d_cost_array);
	return (-1);
}

void tests()
{
  /**/
  vertexid_t aa = 1;
  vertexid_t bb = 2;
  assert(1 == amin(aa,bb));

  vertexid_t* V_minus_S_array_test;
  printf("\n----------------Checking V-S---------------\n");
  int s_array_ctr;
  const int test_ctr = 5;
  unsigned long long a1[test_ctr] = {1,2,3,4};
  unsigned long long a2[test_ctr] = {1,0,0,0};
  V_minus_S_array_test = get_min_compare_vertex_lists(a1,a2,test_ctr);
  unsigned long long min = find_min_in_array(V_minus_S_array_test,test_ctr); // Used for debugging this function.
  for(int i = 0; i < test_ctr; i++){
    printf("V-S array: %llu\n",V_minus_S_array_test[i]);
  }
  printf("min value: %llu", min);
  assert(min == 2);
  printf("\n------------END Checking V-S---------------\n"); //Debugging V-S array building with min function.
  free(V_minus_S_array_test);
}
