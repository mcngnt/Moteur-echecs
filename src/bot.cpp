#include "bot.h"



Bot::Bot()
{
}


int Bot::getDurationFromStart()
{
	std::chrono::high_resolution_clock::time_point endTime = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

int Bot::pieceTableEval(BoardManager* board)
{
	int middleGame = 0;
	int endGame = 0;
	phase = 0;

	for (int color = 0; color <= 1; ++color)
    {
        for (int pType = 1; pType <= 6; ++pType)
        {
            uint64_t pMask = board->getPieceBitboard(pType, color == 0);
            while (pMask != 0)
            {
                int sq = getAndClearLSB(&pMask);

                int pscore = pieceValues[pType - 1];
                phase += piecePhaseValue[pType - 1];
                if (color == 0)
                {
                	middleGame += pieceTable[pType - 1][sq] + pscore;
                	endGame += pieceTable[pType - 1 + 6][sq] + pscore;
                }
                else
                {
                	middleGame -= pieceTable[pType - 1][sq ^ 56] + pscore;
                	endGame -= pieceTable[pType - 1 + 6][sq ^ 56] + pscore;
                }
            }
        }
    }


	return (middleGame * phase + endGame * (24 - phase)) / 24 * (board->whiteToMove ? 1 : -1);
}

int Bot::closeKingEval(BoardManager * board)
{
	int whiteDist = abs((board->currentGameState.whiteKingPos / 8) - 3) + abs((board->currentGameState.whiteKingPos % 8) - 3);
	int blackDist = abs((board->currentGameState.blackKingPos / 8) - 3) + abs((board->currentGameState.blackKingPos % 8) - 3);

	return -(whiteDist - blackDist) * (24 - phase) * (board->whiteToMove ? 1 : -1);
}

int Bot::squareControlEval(BoardManager * board)
{
	return -setBitsNB(board->attackMap);
}



int Bot::evaluate(BoardManager* board)
{

	int pieceScore = pieceTableEval(board);
	int checkScore = (board->inCheck ? -1 : 0);
	// int closeKingScore = closeKing(board);
	// int squareControlScore = squareControlEval(board);

	return pieceScore + checkScore * 100;

	
}


int Bot::search(BoardManager* board, int alpha, int beta, int depth, int plyFromRoot)
 {
 	nodes++;

 	if(board->isRepetitionDraw() && plyFromRoot > 0)
 	{
 		return 0;
 	}

 	Transposition ttEntry = transpositionTable.get(board->zobristKey, depth, alpha, beta);

    bool inQuiescence = depth <= 0;
    bool inCheck = board->isChecked();
    // bool canPrune = false;
    int bestEval = -999999;
    int currentNodeType = AlphaNode;
    int bestMove = 0;
    int eval = 0;

    if (depth > 2 && getDurationFromStart() > maxTime)
    {
        return 999999;
    }


    if (plyFromRoot > 0 && ttEntry.isValid)
    {
        return ttEntry.value;
    }

    if (inCheck)
    {
        depth++;
    }

    if (inQuiescence)
    {
        bestEval = evaluate(board);
        if (bestEval > alpha)
            alpha = bestEval;
        if (alpha >= beta)
            return bestEval;
    }
    else if (!inCheck && beta - alpha == 1)
    {
        // if (depth <= 6 && Evaluate() - depth * 165 > beta)
            // return beta;

        // canPrune = depth <= 6 && evaluate(board) + depth * 165 < alpha;


        if (depth >= 3)
        {
            board->makeNullMove();
            int eval = -search(board,-beta, -alpha, depth - 3, plyFromRoot + 1);
            board->unmakeNullMove();
            if (eval > beta)
            {
                return beta;
            }
        }


    }


    std::vector<int> moves = board->generateMoves(inQuiescence && !inCheck);

    if (moves.size() == 0)
	{
		if (inCheck)
		{
			return plyFromRoot - 999999;
		}
		if (!inQuiescence)
		{
			return 0;
		}
	}

	std::vector<std::pair<int, int>> moveScorePair(moves.size());
	for (int i = 0; i < (int)moves.size(); ++i){
		int move = moves[i];
	    moveScorePair[i] = std::make_pair(move, 

	    	-(

	    		move == ttEntry.bestMove ? 9000000 : getTag(move) == Capture ? 1000000 * pieceType(board->get(endPos(move))) - pieceType(board->get(startPos(move))) : killerMoves[plyFromRoot] == move ? 900000 : historyHeuristicTable[(board->whiteToMove ? 1 : 0)][startPos(move)][endPos(move)]

	    	 )

	   );
	}

	sort(moveScorePair.begin(), moveScorePair.end(), moveOrdering());



    for (int moveCount = 0; moveCount < (int)moves.size(); ++moveCount)
	{

		int move = moveScorePair[moveCount].first;



        // if (canPrune && moveCount > 1 && getTag(move) != Capture && getTag(move) != QueenProm)
        // {
        //      continue;
        // }



		board->makeMove(move);



		if( moveCount == 0 || inQuiescence )
		{
			eval = -search(board,-beta, -alpha, depth - 1, plyFromRoot + 1);
		}
		else
		{
			if(moveCount >= 4 && depth >= 2)
			{
				eval = -search(board,-alpha - 1, -alpha, depth - 2, plyFromRoot + 1);
			}
			else
			{
				eval = alpha + 1;
			}

			if(eval > alpha)
			{
				eval = -search(board,-alpha - 1, -alpha, depth - 1, plyFromRoot + 1);
				if (eval > alpha)
				{
					eval = -search(board,-beta, -alpha, depth - 1, plyFromRoot + 1);
				}
			}
		}



		board->unmakeMove(move);



        if (eval > bestEval)
        {

             bestEval = eval;


             if (eval > alpha)
             {
                 currentNodeType = ExactNode;
                 bestMove = move;
                 alpha = eval;
                 if (plyFromRoot == 0)
                 {
                     rootMove = bestMove;
                 }
             }


             if (alpha >= beta)
             {
                 currentNodeType = BetaNode;
                 if (getTag(move) != Capture)
                 {
                     killerMoves[plyFromRoot] = move;
                     historyHeuristicTable[(board->whiteToMove ? 1 : 0)][startPos(move)][endPos(move)] += depth * depth;
                     //counterMoves[isWhiteMult,(int)previousMove.MovePieceType, previousMove.TargetSquare.Index] = move;
                 }
                 break;
             }

         }
     }


     // entry = new(board.ZobristKey, bestEval, depth, bestMove == default ? entry.bestMove : bestMove, currentFlag);

     transpositionTable.set(board->zobristKey, depth, bestEval, currentNodeType, bestMove == 0 ? ttEntry.bestMove : bestMove);

     return bestEval;

}






int Bot::play(BoardManager* board)
{

	startTime = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < 2; ++i)
	{
		for (int j = 0; j < 64; ++j)
		{
			for (int k = 0; k < 64; ++k)
			{
				historyHeuristicTable[i][j][k] = 0;
			}
		}
	}

	int alpha = -999999;
	int beta = 999999;

	for (int d = 1; d < 90; d++)
	{
	    nodes = 0;
	    int eval = search(board, alpha, beta, d, 0);
	            
	    if (getDurationFromStart() > maxTime)
	    {
	    	break;
	    }

	  	std::cout << "info Depth : " << d << "  ||  Eval : " << eval << "  ||  Nodes : " << nodes << " || Best Move : " << standardNotation(rootMove) << std::endl;


	}

	std::fstream file("C:/Users/marti/Documents/Grenouille/errorFens.txt", std::ios::app);
	file << board->convertFen() << " : " << standardNotation(rootMove) << std::endl;


	std::cout << "info bestmove : " << standardNotation(rootMove) << std::endl;


	std::cout << "info --------------------------------------" << std::endl;

	return rootMove;

}
