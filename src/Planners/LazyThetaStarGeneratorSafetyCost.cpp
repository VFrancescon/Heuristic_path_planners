#include "Planners/LazyThetaStarGeneratorSafetyCost.hpp"

namespace Planners
{

    void LazyThetaStarGeneratorSafetyCost::SetVertex(Node *_s_aux)
    {   
        if (!LineOfSight::bresenham3DWithMaxThreshold((_s_aux->parent), _s_aux, discrete_world_, max_line_of_sight_cells_ ))
        {
            unsigned int G_max = std::numeric_limits<unsigned int>::max(); 
            unsigned int G_new;

            for (const auto &i: direction)
            {
                Vec3i newCoordinates(_s_aux->coordinates + i);

                if ( discrete_world_.isOccupied(newCoordinates) ) continue;

                if ( discrete_world_.isInClosedList(newCoordinates) )
                {
                    Node *successor2 = discrete_world_.getNodePtr(newCoordinates);
                    if (successor2 == nullptr) continue;

                    G_new = successor2->G +  geometry::distanceBetween2Nodes(successor2, _s_aux) + static_cast<int>(cost_weight_ * successor2->cost);
                    if (G_new < G_max)
                    {
                        G_max = G_new;
                        _s_aux->parent = successor2;
                        _s_aux->G = G_new;
                    }
                }
            }
        }
    }
    void LazyThetaStarGeneratorSafetyCost::ComputeCost(Node *_s_aux, Node *_s2_aux)
    {
        auto distanceParent2 = geometry::distanceBetween2Nodes(_s_aux->parent, _s2_aux);

        // Compute cost considering the safety cost.

        if ((_s_aux->parent->G + distanceParent2 ) < (_s2_aux->G))
        {
            _s2_aux->parent = _s_aux->parent;
            _s2_aux->G = _s2_aux->parent->G + geometry::distanceBetween2Nodes(_s2_aux->parent, _s2_aux) +  static_cast<int>(cost_weight_ * _s_aux->cost);
        }
    }

    PathData LazyThetaStarGeneratorSafetyCost::findPath(const Vec3i &_source, const Vec3i &_target)
    {
        Node *current = nullptr;
        NodeSet openSet, closedSet;
        bool solved{false};

        float factor_cost = 1.4142;
        float factor_cost2 = 1.73;

        openSet.insert(discrete_world_.getNodePtr(_source));

        discrete_world_.getNodePtr(_source)->parent = new Node(_source);
        discrete_world_.setOpenValue(_source, true);

        utils::Clock main_timer;
        main_timer.tic();

        int line_of_sight_checks{0};

        while (!openSet.empty())
        {
            float aa, bb;
            aa=0;

            current = *openSet.begin();

            if (current->coordinates == _target)
            {
                solved = true;
                break;
            }

            openSet.erase(openSet.begin());
            closedSet.insert(current);

            discrete_world_.setOpenValue(*current, false);
            discrete_world_.setClosedValue(*current, true);

            aa=current->cost;

            SetVertex(current);
            //in every setVertex the line of sight function is called 
            line_of_sight_checks++;
#if defined(ROS) && defined(PUB_EXPLORED_NODES)
            publishROSDebugData(current, openSet, closedSet);
#endif

            for (unsigned int i = 0; i < direction.size(); ++i)
            {

                Vec3i newCoordinates(current->coordinates + direction[i]);
                float edge_neighbour = 0;
                bb=0;

                if (discrete_world_.isOccupied(newCoordinates) ||
                    discrete_world_.isInClosedList(newCoordinates))
                    continue;
                Node *successor = discrete_world_.getNodePtr(newCoordinates);

                if (successor == nullptr)
                    continue;

                if (!discrete_world_.isInOpenList(newCoordinates))
                {
                    unsigned int totalCost = current->G;

                    if(direction.size()  == 8){
                        totalCost += (i < 4 ? dist_scale_factor_ : dd_2D_); //This is more efficient
                        // Method 1
                        // bb=successor->cost;
                        // Method 2
                        if (totalCost > 100) {
                            bb=(successor->cost)/(factor_cost);
                            // std::cout << "Cost scaled " << bb << " : " << std::endl;
                        }
                        else {
                            bb=successor->cost;
                        }

                        edge_neighbour = (((aa+bb)/(2*100))*totalCost);

                    }else{
                        totalCost += (i < 6 ? dist_scale_factor_ : (i < 18 ? dd_2D_ : dd_3D_)); //This is more efficient
                        // Method 1
                        // bb=successor->cost;
                        // Method 2
                        if ((totalCost > 100) && (totalCost < 150)) {
                            bb=(successor->cost)/(factor_cost);
                            // std::cout << "Cost scaled " << bb << " : " << std::endl;
                        }
                        else if ((totalCost > 150) && (totalCost < 200)){
                            bb=(successor->cost)/(factor_cost2);
                        }
                        else {
                            bb=successor->cost;
                        }

                        edge_neighbour = (((aa+bb)/(2*100))*totalCost);
                    }

                    successor->parent = current;
                    //if (successor->cost >0) std::cout << "Successor Cost " << successor->cost << " : " << std::endl;
                    //successor->G = totalCost + static_cast<int>(cost_weight_ * successor->cost);
                    //successor->G = totalCost + successor->parent->G + static_cast<int>(cost_weight_ * successor->cost);
                    successor->G = current->G + totalCost + edge_neighbour; // This is the same than A*
                    successor->H = heuristic(successor->coordinates, _target);
                    openSet.insert(successor);
                    //discrete_world_.setOpenValue(successor->coordinates, true);
                    discrete_world_.setOpenValue(*successor, true);
                }
                UpdateVertex(current, successor, openSet);
            }
        }
        main_timer.toc();

        PathData result_data;
        result_data["solved"] = solved;

        CoordinateList path;
        if (solved)
        {
            while (current != nullptr)
            {
                path.push_back(current->coordinates);
                current = current->parent;
            }
        }
        else
        {
            std::cout << "Error impossible to calcualte a solution" << std::endl;
        }
        result_data["algorithm"] = std::string("lazythetastar");
        result_data["path"] = path;
        result_data["time_spent"] = main_timer.getElapsedMillisecs();
        result_data["explored_nodes"] = closedSet.size();
        result_data["start_coords"] = _source;
        result_data["goal_coords"] = _target;
        result_data["path_length"] = geometry::calculatePathLength(path, discrete_world_.getResolution());
        result_data["line_of_sight_checks"] = line_of_sight_checks;

#if defined(ROS) && defined(PUB_EXPLORED_NODES)
        explored_node_marker_.points.clear();
#endif

        discrete_world_.resetWorld();
        return result_data;
    }

}